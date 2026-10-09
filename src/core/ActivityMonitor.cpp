#include "core/ActivityMonitor.h"
#include "core/CategoryResolver.h"
#include "core/ExclusionMatcher.h"

#include <algorithm>
#include <exception>
#include <thread>
#include <vector>

namespace pcat {
namespace { constexpr auto checkpointInterval = std::chrono::seconds(30); }

ActivityMonitor::ActivityMonitor(IActivityProvider& provider,IActivityRepository& repository,AppSettings settings,Clock clock)
    :provider_(provider),repository_(repository),clock_(std::move(clock)),settings_(std::move(settings)),categoryIndex_(buildCategoryIndex(settings_.categories)){}
ActivityMonitor::~ActivityMonitor() noexcept { stop(); }

void ActivityMonitor::start(){
    if(running_.exchange(true)) return;
    try {
        thread_=std::jthread([this](std::stop_token token){loop(token);});
    } catch (...) {
        running_.store(false);
        throw;
    }
}

void ActivityMonitor::stop() noexcept {
    const bool wasRunning=running_.exchange(false);
    try {
        if(wasRunning && thread_.joinable()){thread_.request_stop();thread_.join();}
        // Always attempt the final close/flush, even on a repeated stop(). A previous stop may have
        // failed because SQLite was temporarily unavailable and left durable work in pending_.
        closeCurrent(clock_());
        flushPending();
    } catch(const std::exception& e) { setError(e.what()); }
    catch(...) { setError("Unknown error while stopping activity monitor"); }
}

void ActivityMonitor::setPaused(bool value){
    if (!value) {
        paused_.store(false);
        return;
    }
    const bool previous=paused_.exchange(true);
    if (!previous) {
        // Wait for an in-flight capture to finish before closing the interval. Without this guard an
        // already-running process() could create a new current interval after the UI had paused us.
        std::scoped_lock processLock(processMutex_);
        closeCurrent(clock_());
    }
}

bool ActivityMonitor::pauseAndFlushForMaintenance() noexcept {
    try {
        paused_.store(true);
        std::scoped_lock processLock(processMutex_);
        closeCurrent(clock_());
        return flushPending();
    } catch(const std::exception& e) {
        setError(e.what());
        return false;
    } catch(...) {
        setError("Unknown error while preparing history maintenance");
        return false;
    }
}
void ActivityMonitor::setSettings(const AppSettings& settings){std::scoped_lock lock(mutex_);settings_=settings;categoryIndex_=buildCategoryIndex(settings_.categories);}
void ActivityMonitor::setRecordCallback(RecordCallback cb){std::scoped_lock lock(mutex_);callback_=std::move(cb);}
std::string ActivityMonitor::lastError() const { std::scoped_lock lock(mutex_);return lastError_; }
void ActivityMonitor::setError(std::string message){std::scoped_lock lock(mutex_);lastError_=std::move(message);}

ActivityRecord ActivityMonitor::makeRecord(const ActivitySnapshot& snapshot,const std::string& category,std::chrono::system_clock::time_point start,std::chrono::system_clock::time_point end) const {
    ActivityRecord record;record.startTime=start;record.endTime=end;record.durationSeconds=std::max<long long>(0,std::chrono::duration_cast<std::chrono::seconds>(end-start).count());record.processName=snapshot.processName;record.windowTitle=snapshot.windowTitle;record.exePath=snapshot.exePath;record.browserUrl=snapshot.browserUrl;record.browserDomain=snapshot.browserDomain;record.isIdle=snapshot.isIdle;record.category=category;return record;
}

std::optional<ActivityRecord> ActivityMonitor::currentRecordPreview(std::chrono::system_clock::time_point now) const {
    std::scoped_lock lock(mutex_);if(!current_)return std::nullopt;auto record=makeRecord(*current_,currentCategory_,currentStart_,observedEnd(now));if(record.durationSeconds<=0)return std::nullopt;return record;
}

std::chrono::system_clock::time_point ActivityMonitor::observedEnd(std::chrono::system_clock::time_point end) const {
    // Bound extrapolation to one polling interval. Long gaps (sleep, capture errors or slow OS APIs)
    // are not evidence that the previously observed application remained active throughout them.
    return std::max(currentStart_,std::min(end,current_->timestamp
        + std::chrono::seconds(std::clamp(settings_.pollingIntervalSeconds,1,60))));
}

ActivityReadSnapshot ActivityMonitor::readSnapshot(std::chrono::system_clock::time_point start,
                                                  std::chrono::system_clock::time_point end) {
    // Block commits, not capture: a slow OS window query must not hold up readers. Transitions
    // can still enqueue a closed interval; pending_ and current_ are copied together below.
    std::scoped_lock persistenceLock(persistenceMutex_);
    ActivityReadSnapshot result;
    result.records=repository_.getRecords(start,end);
    std::scoped_lock lock(mutex_);
    for(const auto& record:pending_) {
        if(record.endTime>start&&record.startTime<end)result.records.push_back(record);
    }
    if(current_) {
        auto record=makeRecord(*current_,currentCategory_,currentStart_,observedEnd(clock_()));
        if(record.durationSeconds>0&&record.endTime>start&&record.startTime<end)result.current=std::move(record);
    }
    return result;
}

void ActivityMonitor::loop(std::stop_token token){
    while(!token.stop_requested()){
        try{process();}catch(const std::exception& e){setError(e.what());}catch(...){setError("Unknown monitoring error");}
        int seconds=2;{std::scoped_lock lock(mutex_);seconds=std::clamp(settings_.pollingIntervalSeconds,1,60);}
        for(int i=0;i<seconds*10&&!token.stop_requested();++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool ActivityMonitor::different(const ActivitySnapshot& a,const ActivitySnapshot& b,const std::string& ca,const std::string& cb)const{
    return a.processName!=b.processName||a.windowTitle!=b.windowTitle||a.browserDomain!=b.browserDomain||a.browserUrl!=b.browserUrl||a.exePath!=b.exePath||a.isIdle!=b.isIdle||ca!=cb;
}

void ActivityMonitor::process(){
    std::scoped_lock processLock(processMutex_);
    const bool pendingPersisted = flushPending();
    if(paused_.load()){closeCurrent(clock_());return;}
    auto snapshot=provider_.capture();snapshot.timestamp=clock_();snapshot.processName=normalizeProcessName(snapshot.processName);
    AppSettings settings;{std::scoped_lock lock(mutex_);settings=settings_;}
    std::optional<std::chrono::system_clock::time_point> gapEnd;
    {
        std::scoped_lock lock(mutex_);
        if(current_&&(snapshot.timestamp<current_->timestamp||snapshot.timestamp-current_->timestamp
            >std::chrono::seconds(std::max(5,3*std::clamp(settings.pollingIntervalSeconds,1,60))))) {
            gapEnd=observedEnd(snapshot.timestamp);
        }
    }
    if(gapEnd)closeCurrent(*gapEnd);
    snapshot.isIdle=snapshot.idleSeconds>=std::max(10,settings.idleThresholdSeconds);
    if(isExcluded(snapshot,settings)){closeCurrent(snapshot.timestamp);return;}
    if(settings.hideBrowserWindowTitles&&isBrowserProcess(snapshot.processName)){snapshot.windowTitle=snapshot.browserDomain.empty()?"Браузер":"Браузер: "+snapshot.browserDomain;snapshot.browserUrl.clear();}
    // Индекс живёт в мониторе и пересчитывается только при смене настроек, поэтому здесь
    // достаточно короткой блокировки и одного поиска вместо линейного обхода карты категорий.
    std::string category;{std::scoped_lock lock(mutex_);category=resolveCategory(snapshot.processName,categoryIndex_);}

    bool transitioned=false;
    {
        std::scoped_lock lock(mutex_);
        if(!current_){current_=snapshot;currentCategory_=category;currentStart_=snapshot.timestamp;lastCheckpoint_=snapshot.timestamp;return;}
        if(different(*current_,snapshot,currentCategory_,category)){
            auto record=makeRecord(*current_,currentCategory_,currentStart_,snapshot.timestamp);if(record.durationSeconds>0)pending_.push_back(std::move(record));
            current_=snapshot;currentCategory_=category;currentStart_=snapshot.timestamp;lastCheckpoint_=snapshot.timestamp;transitioned=true;
        } else {
            current_->timestamp=snapshot.timestamp;
        }
    }
    if(transitioned) {
        flushPending();
    } else if(pendingPersisted) {
        // Do not overwrite the last durable checkpoint while older intervals are still pending.
        checkpointCurrent(snapshot.timestamp);
    }
}

bool ActivityMonitor::flushPending(){
    // flushPending() can be entered by the capture thread and by pause/stop paths. Serializing the
    // whole persistence loop prevents two callers from inserting the same pending_.front().
    std::scoped_lock persistenceLock(persistenceMutex_);
    while(true){
        ActivityRecord record;RecordCallback callback;
        {
            std::scoped_lock lock(mutex_);if(pending_.empty())return true;record=pending_.front();callback=callback_;
        }
        try{
            repository_.commitClosedRecord(record);
        }catch(const std::exception& e){setError(e.what());return false;}catch(...){setError("Unknown database write error");return false;}
        {
            std::scoped_lock lock(mutex_);if(!pending_.empty())pending_.pop_front();lastError_.clear();
        }
        if(callback)callback(record);
    }
}

void ActivityMonitor::checkpointCurrent(std::chrono::system_clock::time_point now){
    ActivityRecord checkpoint;
    {
        std::scoped_lock lock(mutex_);if(!current_||now-lastCheckpoint_<checkpointInterval)return;checkpoint=makeRecord(*current_,currentCategory_,currentStart_,now);
    }
    if(checkpoint.durationSeconds<=0)return;
    try{repository_.saveOpenCheckpoint(checkpoint);std::scoped_lock lock(mutex_);lastCheckpoint_=now;lastError_.clear();}
    catch(const std::exception& e){setError(e.what());}catch(...){setError("Unknown checkpoint error");}
}

void ActivityMonitor::closeCurrent(std::chrono::system_clock::time_point end) noexcept {
    try{
        {
            std::scoped_lock lock(mutex_);if(!current_)return;auto record=makeRecord(*current_,currentCategory_,currentStart_,observedEnd(end));if(record.durationSeconds>0)pending_.push_back(std::move(record));current_.reset();currentCategory_.clear();lastCheckpoint_={};
        }
        flushPending();
    }catch(const std::exception& e){setError(e.what());}catch(...){setError("Unknown error while closing activity interval");}
}
}
