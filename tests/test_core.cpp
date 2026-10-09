#include "core/ActivityMonitor.h"
#include "core/BrowserAddress.h"
#include "core/CategoryResolver.h"
#include "core/ExclusionMatcher.h"
#include "core/TextUtil.h"
#include "core/TimeUtil.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace pcat;

namespace {
class ConstantProvider final : public IActivityProvider {
public:
    ActivitySnapshot capture() override {
        ActivitySnapshot s;
        s.processName="code.exe";
        s.windowTitle="Editor";
        return s;
    }
    ProviderCapabilities capabilities() const override { return {}; }
};

class RetryRepository final : public IActivityRepository {
public:
    std::atomic_int failuresRemaining{2};
    std::vector<ActivityRecord> records;
    void insert(const ActivityRecord& record) override { records.push_back(record); }
    void commitClosedRecord(const ActivityRecord& record) override {
        if (failuresRemaining-- > 0) throw std::runtime_error("temporary database failure");
        records.push_back(record);
    }
    std::vector<ActivityRecord> getRecords(std::chrono::system_clock::time_point,std::chrono::system_clock::time_point) override { return records; }
    int deleteByFilter(std::chrono::system_clock::time_point,std::chrono::system_clock::time_point,const std::string&,const std::string&,const std::string&) override { return 0; }
    void deleteAll() override { records.clear(); }
};

class ScriptedProvider final : public IActivityProvider {
public:
    std::atomic_int captures{};
    std::atomic_llong seconds{};
    std::chrono::system_clock::time_point base=timeutil::parseIso8601("2026-10-09T08:00:00Z");
    ActivitySnapshot capture() override {
        const int step=captures.load();
        if(step<3)seconds.store(step==0?0:step==1?2:3602);
        ActivitySnapshot snapshot;
        snapshot.processName="chrome.exe";
        snapshot.windowTitle="Same title";
        snapshot.browserDomain="example.com";
        snapshot.browserUrl=step==0?"https://example.com/a":"https://example.com/b";
        captures.fetch_add(1);
        return snapshot;
    }
    ProviderCapabilities capabilities() const override { return {}; }
};

void waitForCaptures(const ScriptedProvider& provider,int count) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(provider.captures.load()<count&&std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(provider.captures.load()>=count);
}
}

int main() {
    assert(textutil::canonicalApplicationId("C:\\Program Files\\Code.EXE") == "code");
    assert(textutil::canonicalApplicationId("/usr/bin/code") == "code");
    assert(textutil::containsCaseInsensitive("ЗАЯВКА на закупку", "заявка"));

    AppSettings settings;
    settings.excludedProcesses = {"code.exe", "firefox"};
    ActivitySnapshot snapshot;
    snapshot.processName = "CODE";
    assert(isExcluded(snapshot, settings));
    snapshot.processName = "/usr/bin/firefox";
    assert(isExcluded(snapshot, settings));
    assert(isBrowserProcess("/usr/bin/google-chrome"));
    assert(isBrowserProcess("Google Chrome"));
    assert(isBrowserProcess("Microsoft Edge"));
    assert(resolveCategory("code", settings.categories) == "Разработка");
    assert(resolveCategory("Visual Studio Code", settings.categories) == "Разработка");
    assert(resolveCategory("/usr/bin/google-chrome", settings.categories) == "Браузер");

    settings.excludedProcesses.clear();
    settings.excludedWindowTitlePatterns = {"*СЕКРЕТ*"};
    snapshot.windowTitle = "Документ — секретный раздел";
    assert(isExcluded(snapshot, settings));
    settings.excludedWindowTitlePatterns = {"Секре?"};
    snapshot.windowTitle = "СЕКРЕТ";
    assert(isExcluded(snapshot, settings)); // '?' matches one Unicode code point, not one UTF-8 byte.

    const auto address = parseBrowserAddress("Example.COM:8443/path?q=1");
    assert(address);
    assert(address->url == "https://Example.COM:8443/path?q=1");
    assert(address->domain == "example.com");
    const auto embedded = findBrowserAddressInText("open HTTPS://Example.org/test now");
    assert(embedded && embedded->domain == "example.org");
    assert(!parseBrowserAddress("not a url"));
    assert(!parseBrowserAddress("file:///tmp/test"));
    assert(!parseBrowserAddress("https://example.com:not-a-port/path"));
    assert(!parseBrowserAddress("https://example.com:70000/path"));
    assert(parseBrowserAddress("https://[::1]:8443/path"));
    const auto embeddedIpv6=findBrowserAddressInText("open https://[::1]:8443/path now");
    assert(embeddedIpv6&&embeddedIpv6->domain=="[::1]");
    assert(!parseBrowserAddress("https://example.com]/path"));
    assert(normalizeDomain("https://user:pass@WWW.Example.com:443/path") == "example.com");
    const auto afterUnicode = findBrowserAddressInText("ẞ символ HTTPS://Example.net/path");
    assert(afterUnicode && afterUnicode->domain == "example.net");

    // .NET DateTime.ToString("O") compatibility: seven fractional digits and an explicit offset.
    const auto dotnet = timeutil::parseIso8601("2026-08-24T10:00:00.1234567+03:00");
    const auto utc = timeutil::parseIso8601("2026-08-24T07:00:00.1234567Z");
    assert(dotnet == utc);
    assert(timeutil::parseIso8601(timeutil::toIso8601Utc(dotnet)) == dotnet);
    const auto beforeEpoch = timeutil::parseIso8601("1969-12-31T23:59:59.5000000Z");
    assert(timeutil::parseIso8601(timeutil::toIso8601Utc(beforeEpoch)) == beforeEpoch);
    bool badOffset=false;
    try { (void)timeutil::parseIso8601("2026-08-24T10:00:00+14:30"); } catch (...) { badOffset=true; }
    assert(badOffset);

    // A transient persistence error during stop must not make the queued interval unrecoverable.
    // A repeated stop() retries pending work even though the capture thread is already stopped.
    {
        ConstantProvider provider;
        RetryRepository repository;
        AppSettings monitorSettings;
        monitorSettings.pollingIntervalSeconds=1;
        monitorSettings.excludedProcesses.clear();
        ActivityMonitor monitor(provider,repository,monitorSettings);
        monitor.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(1150));
        monitor.stop();
        assert(repository.records.empty());
        assert(!monitor.lastError().empty());
        monitor.stop();
        assert(repository.records.size()==1);
        assert(repository.records[0].durationSeconds>=1);
    }

    // Navigation with an unchanged title/domain still closes the old URL interval. A simulated
    // one-hour suspend must leave a gap, and previews/pause/stop must not extrapolate through it.
    {
        ScriptedProvider provider;
        RetryRepository repository;repository.failuresRemaining=0;
        AppSettings monitorSettings;monitorSettings.pollingIntervalSeconds=1;monitorSettings.hideBrowserWindowTitles=false;
        ActivityMonitor monitor(provider,repository,monitorSettings,[&]{return provider.base+std::chrono::seconds(provider.seconds.load());});
        monitor.start();
        waitForCaptures(provider,3);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(std::chrono::steady_clock::now()<deadline) {
            const auto current=monitor.currentRecordPreview(provider.base+std::chrono::hours(2));
            if(current&&current->startTime==provider.base+std::chrono::seconds(3602))break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto snapshot=monitor.readSnapshot(provider.base,provider.base+std::chrono::hours(2));
        assert(snapshot.records.size()==2);
        assert(snapshot.records[0].browserUrl=="https://example.com/a"&&snapshot.records[0].durationSeconds==2);
        assert(snapshot.records[1].browserUrl=="https://example.com/b"&&snapshot.records[1].durationSeconds==1);
        const auto preview=monitor.currentRecordPreview(provider.base+std::chrono::hours(2));
        assert(preview&&preview->startTime==provider.base+std::chrono::seconds(3602)&&preview->durationSeconds==1);
        // Close while writes fail: the read snapshot must still include queued records exactly once.
        repository.failuresRemaining=100;
        provider.seconds.store(3603);
        monitor.setPaused(true);
        snapshot=monitor.readSnapshot(provider.base,provider.base+std::chrono::hours(2));
        assert(!snapshot.current&&snapshot.records.size()==3);
        long long total=0;for(const auto& record:snapshot.records)total+=record.durationSeconds;
        assert(total==4);
        // Stop joins capture before changing the fake repository's retry policy.
        monitor.stop();repository.failuresRemaining=0;monitor.stop();
        snapshot=monitor.readSnapshot(provider.base,provider.base+std::chrono::hours(2));
        assert(!snapshot.current&&snapshot.records.size()==3);
        assert(monitor.lastError().empty());
    }

    std::cout << "core tests passed\n";
}
