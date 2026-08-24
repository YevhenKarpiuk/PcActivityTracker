#include "data/ActivityRepository.h"
#include "core/TextUtil.h"
#include "core/TimeUtil.h"

#include <sqlite3.h>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace pcat {
namespace {
void check(int rc, sqlite3* db) {
    if (rc!=SQLITE_OK && rc!=SQLITE_DONE && rc!=SQLITE_ROW) throw std::runtime_error(db?sqlite3_errmsg(db):"SQLite error");
}
struct StatementDeleter { void operator()(sqlite3_stmt* statement) const noexcept { if(statement) sqlite3_finalize(statement); } };
using Statement = std::unique_ptr<sqlite3_stmt,StatementDeleter>;
Statement prepare(sqlite3* db,const char* sql) { sqlite3_stmt* raw=nullptr;check(sqlite3_prepare_v2(db,sql,-1,&raw,nullptr),db);return Statement(raw); }
long long epochMillis(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}
std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}
std::string columnText(sqlite3_stmt* s,int column) { const auto* p=reinterpret_cast<const char*>(sqlite3_column_text(s,column));return p?std::string(p):std::string{}; }
void bindText(sqlite3_stmt* s,int index,const std::string& value) { check(sqlite3_bind_text(s,index,value.c_str(),-1,SQLITE_TRANSIENT),sqlite3_db_handle(s)); }
void begin(sqlite3* db){char*err=nullptr;const auto rc=sqlite3_exec(db,"BEGIN IMMEDIATE;",nullptr,nullptr,&err);if(rc!=SQLITE_OK){std::string msg=err?err:"SQLite BEGIN failed";sqlite3_free(err);throw std::runtime_error(msg);}}
void rollbackNoThrow(sqlite3* db){sqlite3_exec(db,"ROLLBACK;",nullptr,nullptr,nullptr);}
}

ActivityRepository::ActivityRepository(std::filesystem::path path):dbPath_(std::move(path)){}
ActivityRepository::~ActivityRepository(){if(db_)sqlite3_close(db_);}

void ActivityRepository::exec(const char* sql){char*err=nullptr;const int rc=sqlite3_exec(db_,sql,nullptr,nullptr,&err);if(rc!=SQLITE_OK){std::string msg=err?err:"SQLite error";sqlite3_free(err);throw std::runtime_error(msg);}}

void ActivityRepository::initialize(){
    std::scoped_lock lock(mutex_);
    if(db_) return;
    if(!dbPath_.parent_path().empty())std::filesystem::create_directories(dbPath_.parent_path());
    const auto dbPathUtf8=pathUtf8(dbPath_);
    const auto rc=sqlite3_open_v2(dbPathUtf8.c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr);
    if(rc!=SQLITE_OK){std::string msg=db_?sqlite3_errmsg(db_):"Cannot open SQLite database";if(db_){sqlite3_close(db_);db_=nullptr;}throw std::runtime_error(msg);}
    try {
    sqlite3_busy_timeout(db_,5000);
    exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON;"
         "CREATE TABLE IF NOT EXISTS activity_records(id INTEGER PRIMARY KEY AUTOINCREMENT,start_time TEXT NOT NULL,end_time TEXT NOT NULL,duration_seconds INTEGER NOT NULL,process_name TEXT,window_title TEXT,exe_path TEXT,browser_url TEXT,browser_domain TEXT,is_idle INTEGER NOT NULL DEFAULT 0,category TEXT);"
         "CREATE TABLE IF NOT EXISTS app_categories(id INTEGER PRIMARY KEY AUTOINCREMENT,process_name TEXT NOT NULL UNIQUE,category TEXT NOT NULL);"
         "CREATE TABLE IF NOT EXISTS app_settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
         "CREATE TABLE IF NOT EXISTS monitor_checkpoint(id INTEGER PRIMARY KEY CHECK(id=1),start_time TEXT NOT NULL,end_time TEXT NOT NULL,start_epoch_ms INTEGER NOT NULL,end_epoch_ms INTEGER NOT NULL,duration_seconds INTEGER NOT NULL,process_name TEXT,window_title TEXT,exe_path TEXT,browser_url TEXT,browser_domain TEXT,is_idle INTEGER NOT NULL DEFAULT 0,category TEXT);");

    const struct {const char*name;const char*type;} columns[]={
        {"exe_path","TEXT"},{"browser_url","TEXT"},{"browser_domain","TEXT"},{"is_idle","INTEGER NOT NULL DEFAULT 0"},{"category","TEXT"},
        {"start_epoch_ms","INTEGER"},{"end_epoch_ms","INTEGER"}
    };
    for(const auto& column:columns){
        bool found=false;auto st=prepare(db_,"PRAGMA table_info(activity_records);");
        int step=SQLITE_OK;while((step=sqlite3_step(st.get()))==SQLITE_ROW){const auto name=columnText(st.get(),1);if(name==column.name){found=true;break;}}
        if(!found&&step!=SQLITE_DONE)check(step,db_);
        if(!found)exec((std::string("ALTER TABLE activity_records ADD COLUMN ")+column.name+" "+column.type+";").c_str());
    }

    // Backfill epoch columns once so range queries can use ordinary INTEGER indexes.
    {
        auto select=prepare(db_,"SELECT id,start_time,end_time FROM activity_records WHERE start_epoch_ms IS NULL OR end_epoch_ms IS NULL;");
        auto update=prepare(db_,"UPDATE activity_records SET start_epoch_ms=?,end_epoch_ms=? WHERE id=?;");
        int selectStep=SQLITE_OK;
        while((selectStep=sqlite3_step(select.get()))==SQLITE_ROW){
            const auto id=sqlite3_column_int64(select.get(),0);const auto start=columnText(select.get(),1);const auto end=columnText(select.get(),2);
            try{
                sqlite3_reset(update.get());sqlite3_clear_bindings(update.get());
                sqlite3_bind_int64(update.get(),1,epochMillis(timeutil::parseIso8601(start)));sqlite3_bind_int64(update.get(),2,epochMillis(timeutil::parseIso8601(end)));sqlite3_bind_int64(update.get(),3,id);check(sqlite3_step(update.get()),db_);
            }catch(...){/* A malformed legacy timestamp is skipped without blocking startup. */}
        }
        check(selectStep,db_);
    }

    exec("CREATE INDEX IF NOT EXISTS idx_activity_records_start_epoch ON activity_records(start_epoch_ms);"
         "CREATE INDEX IF NOT EXISTS idx_activity_records_end_epoch ON activity_records(end_epoch_ms);"
         "CREATE INDEX IF NOT EXISTS idx_activity_records_process_name ON activity_records(process_name);"
         "CREATE INDEX IF NOT EXISTS idx_activity_records_category ON activity_records(category);"
         "CREATE INDEX IF NOT EXISTS idx_activity_records_browser_domain ON activity_records(browser_domain);");

    // Recover the most recent checkpoint after an unclean shutdown. The NOT EXISTS guard prevents
    // duplication if the final interval was inserted but the checkpoint could not be cleared.
    begin(db_);
    try{
        exec("INSERT INTO activity_records(start_time,end_time,start_epoch_ms,end_epoch_ms,duration_seconds,process_name,window_title,exe_path,browser_url,browser_domain,is_idle,category) "
             "SELECT c.start_time,c.end_time,c.start_epoch_ms,c.end_epoch_ms,c.duration_seconds,c.process_name,c.window_title,c.exe_path,c.browser_url,c.browser_domain,c.is_idle,c.category FROM monitor_checkpoint c "
             "WHERE c.duration_seconds>0 AND NOT EXISTS(SELECT 1 FROM activity_records a WHERE a.start_epoch_ms=c.start_epoch_ms AND a.end_epoch_ms>=c.end_epoch_ms AND coalesce(a.process_name,'')=coalesce(c.process_name,'') AND coalesce(a.window_title,'')=coalesce(c.window_title,'') AND a.is_idle=c.is_idle);"
             "DELETE FROM monitor_checkpoint;COMMIT;");
    }catch(...){rollbackNoThrow(db_);throw;}
    } catch (...) {
        sqlite3_close(db_);
        db_=nullptr;
        throw;
    }
}

void ActivityRepository::insert(const ActivityRecord& record){
    std::scoped_lock lock(mutex_);
    auto statement=prepare(db_,"INSERT INTO activity_records(start_time,end_time,start_epoch_ms,end_epoch_ms,duration_seconds,process_name,window_title,exe_path,browser_url,browser_domain,is_idle,category) VALUES(?,?,?,?,?,?,?,?,?,?,?,?);");
    const auto start=timeutil::toIso8601Utc(record.startTime),end=timeutil::toIso8601Utc(record.endTime);
    bindText(statement.get(),1,start);bindText(statement.get(),2,end);sqlite3_bind_int64(statement.get(),3,epochMillis(record.startTime));sqlite3_bind_int64(statement.get(),4,epochMillis(record.endTime));sqlite3_bind_int64(statement.get(),5,record.durationSeconds);
    bindText(statement.get(),6,record.processName);bindText(statement.get(),7,record.windowTitle);bindText(statement.get(),8,record.exePath);bindText(statement.get(),9,record.browserUrl);bindText(statement.get(),10,record.browserDomain);sqlite3_bind_int(statement.get(),11,record.isIdle?1:0);bindText(statement.get(),12,record.category);
    check(sqlite3_step(statement.get()),db_);
}

void ActivityRepository::commitClosedRecord(const ActivityRecord& record){
    std::scoped_lock lock(mutex_);
    begin(db_);
    try {
        auto statement=prepare(db_,"INSERT INTO activity_records(start_time,end_time,start_epoch_ms,end_epoch_ms,duration_seconds,process_name,window_title,exe_path,browser_url,browser_domain,is_idle,category) VALUES(?,?,?,?,?,?,?,?,?,?,?,?);");
        const auto start=timeutil::toIso8601Utc(record.startTime),end=timeutil::toIso8601Utc(record.endTime);
        bindText(statement.get(),1,start);bindText(statement.get(),2,end);sqlite3_bind_int64(statement.get(),3,epochMillis(record.startTime));sqlite3_bind_int64(statement.get(),4,epochMillis(record.endTime));sqlite3_bind_int64(statement.get(),5,record.durationSeconds);
        bindText(statement.get(),6,record.processName);bindText(statement.get(),7,record.windowTitle);bindText(statement.get(),8,record.exePath);bindText(statement.get(),9,record.browserUrl);bindText(statement.get(),10,record.browserDomain);sqlite3_bind_int(statement.get(),11,record.isIdle?1:0);bindText(statement.get(),12,record.category);
        check(sqlite3_step(statement.get()),db_);
        exec("DELETE FROM monitor_checkpoint;COMMIT;");
    } catch (...) {
        rollbackNoThrow(db_);
        throw;
    }
}

std::vector<ActivityRecord> ActivityRepository::getRecords(std::chrono::system_clock::time_point start,std::chrono::system_clock::time_point end){
    std::scoped_lock lock(mutex_);std::vector<ActivityRecord> result;
    auto s=prepare(db_,"SELECT id,start_time,end_time,duration_seconds,process_name,window_title,exe_path,browser_url,browser_domain,is_idle,category FROM activity_records WHERE end_epoch_ms>? AND start_epoch_ms<? ORDER BY start_epoch_ms DESC;");
    sqlite3_bind_int64(s.get(),1,epochMillis(start));sqlite3_bind_int64(s.get(),2,epochMillis(end));
    int step=SQLITE_OK;while((step=sqlite3_step(s.get()))==SQLITE_ROW){ActivityRecord r;r.id=sqlite3_column_int64(s.get(),0);r.startTime=timeutil::parseIso8601(columnText(s.get(),1));r.endTime=timeutil::parseIso8601(columnText(s.get(),2));r.durationSeconds=sqlite3_column_int64(s.get(),3);r.processName=columnText(s.get(),4);r.windowTitle=columnText(s.get(),5);r.exePath=columnText(s.get(),6);r.browserUrl=columnText(s.get(),7);r.browserDomain=columnText(s.get(),8);r.isIdle=sqlite3_column_int(s.get(),9)!=0;r.category=columnText(s.get(),10);if(r.category.empty())r.category="Без категории";result.push_back(std::move(r));}
    check(step,db_);return result;
}

int ActivityRepository::deleteByFilter(std::chrono::system_clock::time_point start,std::chrono::system_clock::time_point end,const std::string& process,const std::string& category,const std::string& title){
    std::scoped_lock lock(mutex_);
    auto select=prepare(db_,"SELECT id,process_name,category,window_title FROM activity_records WHERE end_epoch_ms>? AND start_epoch_ms<?;");
    sqlite3_bind_int64(select.get(),1,epochMillis(start));sqlite3_bind_int64(select.get(),2,epochMillis(end));
    std::vector<sqlite3_int64> ids;
    int selectStep=SQLITE_OK;while((selectStep=sqlite3_step(select.get()))==SQLITE_ROW){
        if(textutil::containsCaseInsensitive(columnText(select.get(),1),process) && textutil::containsCaseInsensitive(columnText(select.get(),2),category) && textutil::containsCaseInsensitive(columnText(select.get(),3),title))ids.push_back(sqlite3_column_int64(select.get(),0));
    }
    check(selectStep,db_);if(ids.empty())return 0;
    begin(db_);int deleted=0;
    try{auto del=prepare(db_,"DELETE FROM activity_records WHERE id=?;");for(auto id:ids){sqlite3_reset(del.get());sqlite3_clear_bindings(del.get());sqlite3_bind_int64(del.get(),1,id);check(sqlite3_step(del.get()),db_);deleted+=sqlite3_changes(db_);}exec("COMMIT;");}
    catch(...){rollbackNoThrow(db_);throw;}
    return deleted;
}

void ActivityRepository::deleteAll(){
    std::scoped_lock lock(mutex_);
    begin(db_);
    try {
        exec("DELETE FROM activity_records;DELETE FROM monitor_checkpoint;COMMIT;");
    } catch (...) {
        rollbackNoThrow(db_);
        throw;
    }
}

void ActivityRepository::saveOpenCheckpoint(const ActivityRecord& record){
    if(record.durationSeconds<=0)return;
    std::scoped_lock lock(mutex_);
    auto s=prepare(db_,"INSERT INTO monitor_checkpoint(id,start_time,end_time,start_epoch_ms,end_epoch_ms,duration_seconds,process_name,window_title,exe_path,browser_url,browser_domain,is_idle,category) VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET start_time=excluded.start_time,end_time=excluded.end_time,start_epoch_ms=excluded.start_epoch_ms,end_epoch_ms=excluded.end_epoch_ms,duration_seconds=excluded.duration_seconds,process_name=excluded.process_name,window_title=excluded.window_title,exe_path=excluded.exe_path,browser_url=excluded.browser_url,browser_domain=excluded.browser_domain,is_idle=excluded.is_idle,category=excluded.category;");
    bindText(s.get(),1,timeutil::toIso8601Utc(record.startTime));bindText(s.get(),2,timeutil::toIso8601Utc(record.endTime));sqlite3_bind_int64(s.get(),3,epochMillis(record.startTime));sqlite3_bind_int64(s.get(),4,epochMillis(record.endTime));sqlite3_bind_int64(s.get(),5,record.durationSeconds);bindText(s.get(),6,record.processName);bindText(s.get(),7,record.windowTitle);bindText(s.get(),8,record.exePath);bindText(s.get(),9,record.browserUrl);bindText(s.get(),10,record.browserDomain);sqlite3_bind_int(s.get(),11,record.isIdle?1:0);bindText(s.get(),12,record.category);check(sqlite3_step(s.get()),db_);
}

void ActivityRepository::clearOpenCheckpoint(){std::scoped_lock lock(mutex_);exec("DELETE FROM monitor_checkpoint;");}
}
