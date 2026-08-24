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
    int failuresRemaining{2};
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

    std::cout << "core tests passed\n";
}
