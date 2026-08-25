#include "data/ActivityRepository.h"
#include "data/SettingsService.h"
#include "data/AppStoragePaths.h"
#include "core/TimeUtil.h"

#include <sqlite3.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace pcat;

static ActivityRecord makeRecord(const char* start,const char* end,const char* process,const char* category) {
    ActivityRecord record;
    record.startTime=timeutil::parseIso8601(start);
    record.endTime=timeutil::parseIso8601(end);
    record.durationSeconds=std::chrono::duration_cast<std::chrono::seconds>(record.endTime-record.startTime).count();
    record.processName=process;
    record.windowTitle="Окно Разработки";
    record.category=category;
    return record;
}


static int scalarCount(const std::filesystem::path& path,const char* sql) {
    sqlite3* db=nullptr;
    const auto utf8=path.u8string();
    const std::string native(reinterpret_cast<const char*>(utf8.data()),utf8.size());
    if(sqlite3_open(native.c_str(),&db)!=SQLITE_OK) throw std::runtime_error("cannot open db for assertion");
    sqlite3_stmt* statement=nullptr;
    if(sqlite3_prepare_v2(db,sql,-1,&statement,nullptr)!=SQLITE_OK){sqlite3_close(db);throw std::runtime_error("cannot prepare assertion query");}
    const int rc=sqlite3_step(statement);
    const int value=rc==SQLITE_ROW?sqlite3_column_int(statement,0):-1;
    sqlite3_finalize(statement);sqlite3_close(db);
    if(value<0) throw std::runtime_error("cannot execute assertion query");
    return value;
}

static void createLegacyDatabase(const std::filesystem::path& path) {
    sqlite3* db=nullptr;
    const auto pathBytes=path.u8string();
    const std::string utf8Path(reinterpret_cast<const char*>(pathBytes.data()),pathBytes.size());
    if(sqlite3_open(utf8Path.c_str(),&db)!=SQLITE_OK) throw std::runtime_error("cannot create legacy db");
    const char* sql=
        "CREATE TABLE activity_records(id INTEGER PRIMARY KEY AUTOINCREMENT,start_time TEXT NOT NULL,end_time TEXT NOT NULL,duration_seconds INTEGER NOT NULL,process_name TEXT,window_title TEXT);"
        "INSERT INTO activity_records(start_time,end_time,duration_seconds,process_name,window_title) VALUES('2026-08-23T20:00:00.0000000+03:00','2026-08-23T20:10:00.0000000+03:00',600,'legacy.exe','Старое окно');";
    char* error=nullptr;
    const int rc=sqlite3_exec(db,sql,nullptr,nullptr,&error);
    const std::string message=error?error:"legacy db error";
    sqlite3_free(error);
    sqlite3_close(db);
    if(rc!=SQLITE_OK) throw std::runtime_error(message);
}

int main() {
    auto dir=std::filesystem::temp_directory_path()/"pcat_cpp_test";
    std::error_code ec;
    std::filesystem::remove_all(dir,ec);
    std::filesystem::create_directories(dir);

    {
        ActivityRepository repo(dir/"activity_tracker.db");
        repo.initialize();
        auto record=makeRecord("2026-08-24T10:00:00.0000000+03:00","2026-08-24T10:15:00.0000000+03:00","code.exe","Разработка");
        repo.insert(record);
        auto rows=repo.getRecords(timeutil::parseIso8601("2026-08-24T00:00:00+03:00"),timeutil::parseIso8601("2026-08-25T00:00:00+03:00"));
        assert(rows.size()==1);
        assert(rows[0].durationSeconds==900);
        assert(rows[0].processName=="code.exe");

        // SQLite lower() is ASCII-only; deletion therefore uses the C++ UTF-8 case-fold path.
        assert(repo.deleteByFilter(timeutil::parseIso8601("2026-08-24T00:00:00+03:00"),timeutil::parseIso8601("2026-08-25T00:00:00+03:00"),"CODE","разработка","окно")==1);
        repo.insert(record);
        auto checkpoint=makeRecord("2026-08-24T11:00:00+03:00","2026-08-24T11:02:00+03:00","firefox","Браузер");
        repo.saveOpenCheckpoint(checkpoint);
    }

    // Opening after an unclean shutdown recovers the checkpoint exactly once.
    {
        ActivityRepository repo(dir/"activity_tracker.db");
        repo.initialize();
        auto rows=repo.getRecords(timeutil::parseIso8601("2026-08-24T00:00:00+03:00"),timeutil::parseIso8601("2026-08-25T00:00:00+03:00"));
        assert(rows.size()==2);
    }
    {
        ActivityRepository repo(dir/"activity_tracker.db");
        repo.initialize();
        auto rows=repo.getRecords(timeutil::parseIso8601("2026-08-24T00:00:00+03:00"),timeutil::parseIso8601("2026-08-25T00:00:00+03:00"));
        assert(rows.size()==2);
    }

    // Closing an interval is one transaction with retiring its crash checkpoint.
    {
        ActivityRepository repo(dir/"activity_tracker.db");
        repo.initialize();
        auto closed=makeRecord("2026-08-24T12:00:00+03:00","2026-08-24T12:03:00+03:00","terminal","Разработка");
        repo.saveOpenCheckpoint(closed);
        assert(scalarCount(dir/"activity_tracker.db","SELECT count(*) FROM monitor_checkpoint;")==1);
        repo.commitClosedRecord(closed);
        assert(scalarCount(dir/"activity_tracker.db","SELECT count(*) FROM monitor_checkpoint;")==0);
    }

    // sqlite3_open_v2 expects UTF-8. Keep database paths with non-ASCII components working,
    // especially for Windows profiles/project folders that contain Cyrillic characters.
    {
        const auto unicodeDir=dir/"данные"/"история";
        std::filesystem::create_directories(unicodeDir);
        ActivityRepository repo(unicodeDir/"активность.db");
        repo.initialize();
        repo.insert(makeRecord("2026-08-24T12:00:00+03:00","2026-08-24T12:01:00+03:00","code","Разработка"));
        const auto rows=repo.getRecords(timeutil::parseIso8601("2026-08-24T00:00:00+03:00"),timeutil::parseIso8601("2026-08-25T00:00:00+03:00"));
        assert(rows.size()==1);
    }

    // SQLite INTEGER is 64-bit: very long legacy/imported durations must not wrap through a 32-bit int.
    {
        ActivityRepository repo(dir/"long-duration.db");
        repo.initialize();
        auto record=makeRecord("2026-08-24T00:00:00Z","2026-08-24T00:00:01Z","imported","Импорт");
        record.durationSeconds=3'000'000'000LL;
        repo.insert(record);
        const auto rows=repo.getRecords(timeutil::parseIso8601("2026-08-23T00:00:00Z"),timeutil::parseIso8601("2026-08-25T00:00:00Z"));
        assert(rows.size()==1);
        assert(rows[0].durationSeconds==3'000'000'000LL);
    }

    // A real legacy schema without the C++ epoch columns must migrate and remain queryable.
    const auto legacyDir=dir/"legacy";
    std::filesystem::create_directories(legacyDir);
    createLegacyDatabase(legacyDir/"activity_tracker.db");
    {
        ActivityRepository repo(legacyDir/"activity_tracker.db");
        repo.initialize();
        const auto rows=repo.getRecords(timeutil::parseIso8601("2026-08-23T00:00:00+03:00"),timeutil::parseIso8601("2026-08-24T00:00:00+03:00"));
        assert(rows.size()==1);
        assert(rows[0].processName=="legacy.exe");
        assert(rows[0].durationSeconds==600);
    }

    // System.Text.Json legacy \\uXXXX sequences, including Cyrillic and a surrogate pair, decode correctly.
    {
        std::ofstream file(dir/"settings.json");
        file<<R"({"IdleThresholdSeconds":77,"PollingIntervalSeconds":3,"HideBrowserWindowTitles":false,"StartWithWindows":true,"ExcludedProcesses":["Code.exe","code","firefox"],"ExcludedWindowTitles":["\u0421\u0435\u043a\u0440\u0435\u0442"],"ExcludedWindowTitlePatterns":["*password*"],"ExcludedBrowserDomains":["https://www.example.com/path"],"Categories":{"code.exe":"\u0420\u0430\u0437\u0440\u0430\u0431\u043e\u0442\u043a\u0430","chat":"emoji \uD83D\uDE00"}})";
    }
    SettingsService settingsService(dir/"settings.json");
    auto settings=settingsService.load();
    assert(settings.idleThresholdSeconds==77);
    assert(settings.pollingIntervalSeconds==3);
    assert(!settings.hideBrowserWindowTitles);
    assert(settings.startWithSystem);
    assert(settings.excludedBrowserDomains.size()==1&&settings.excludedBrowserDomains[0]=="example.com");
    assert(settings.excludedProcesses.size()==2);
    assert(settings.categories.at("code.exe")=="Разработка");
    assert(settings.categories.at("chat")=="emoji 😀");
    assert(settings.excludedWindowTitles.at(0)=="Секрет");
    settingsService.save(settings);
    const auto reloaded=settingsService.load();
    assert(reloaded.startWithSystem&&reloaded.idleThresholdSeconds==77&&reloaded.categories.at("code.exe")=="Разработка");

    // Explicit empty collections mean empty: they must not silently restore built-in defaults.
    {
        std::ofstream file(dir/"settings.json");
        file<<R"({"ExcludedProcesses":[],"Categories":{},"ExcludedWindowTitles":[],"ExcludedWindowTitlePatterns":[],"ExcludedBrowserDomains":[]})";
    }
    const auto empty=settingsService.load();
    assert(empty.excludedProcesses.empty());
    assert(empty.categories.empty());

    // portable.txt always keeps the database/settings next to the executable, not one directory above.
    {
        const auto exeDir=dir/"portable-app";
        std::filesystem::create_directories(exeDir);
        std::ofstream(exeDir/"portable.txt") << "portable\n";
        assert(appDataDirectory(exeDir) == exeDir/"data");
    }

    // A writable program directory is the default location even without portable.txt.
    {
        const auto exeDir=dir/"plain-app";
        std::filesystem::create_directories(exeDir);
        assert(!std::filesystem::exists(exeDir/"portable.txt"));
        assert(appDataDirectory(exeDir) == exeDir/"data");
        // The probe file used to detect writability must not be left behind.
        assert(!std::filesystem::exists(exeDir/"data"/".pcat-write-test"));
    }

    // Existing data next to the executable keeps winning, so an upgrade never loses history.
    {
        const auto exeDir=dir/"legacy-app";
        std::filesystem::create_directories(exeDir/"data");
        std::ofstream(exeDir/"data"/"activity_tracker.db") << "";
        assert(appDataDirectory(exeDir) == exeDir/"data");
    }

    // UTF-8 BOM from common Windows editors is accepted.
    {
        std::ofstream file(dir/"settings.json",std::ios::binary|std::ios::trunc);
        file << "\xEF\xBB\xBF" << R"({"IdleThresholdSeconds":91})";
    }
    assert(settingsService.load().idleThresholdSeconds==91);

    // A wrong type in one field must not discard every other user setting. The field falls back to its
    // default, the out-of-range cast is still avoided, the file is left untouched and the problem is
    // reported through lastLoadWarnings().
    {
        std::ofstream file(dir/"settings.json",std::ios::binary|std::ios::trunc);
        file << R"({"IdleThresholdSeconds":2147483648,"PollingIntervalSeconds":"two","ExcludedProcesses":["keep.exe"]})";
    }
    const auto tolerated=settingsService.load();
    assert(tolerated.idleThresholdSeconds==60);
    assert(tolerated.pollingIntervalSeconds==2);
    assert(tolerated.excludedProcesses.size()==1&&tolerated.excludedProcesses[0]=="keep.exe");
    assert(settingsService.lastLoadWarnings().size()==2);
    { auto corrupt=dir/"settings.json";corrupt += ".corrupt";assert(!std::filesystem::exists(corrupt)); }

    // Structural damage is still corruption: keep a copy of the file and fall back to defaults.
    {
        std::ofstream file(dir/"settings.json",std::ios::binary|std::ios::trunc);
        file << "{ this is not json";
    }
    const auto repaired=settingsService.load();
    assert(repaired.idleThresholdSeconds==60);
    assert(repaired.excludedProcesses.size()==AppSettings{}.excludedProcesses.size());
    { auto corrupt=dir/"settings.json";corrupt += ".corrupt";assert(std::filesystem::exists(corrupt)); }

    // An I/O failure is not a parse failure: do not overwrite an unreadable settings path with defaults.
    {
        const auto ioPath=dir/"settings-io.json";
        std::filesystem::create_directory(ioPath);
        SettingsService ioService(ioPath);
        bool threw=false;
        try { (void)ioService.load(); } catch (...) { threw=true; }
        assert(threw);
        assert(std::filesystem::is_directory(ioPath));
        threw=false;
        try { ioService.save(AppSettings{}); } catch (...) { threw=true; }
        assert(threw);
        assert(std::filesystem::is_directory(ioPath));
    }

    std::filesystem::remove_all(dir,ec);
    std::cout<<"data tests passed\n";
}
