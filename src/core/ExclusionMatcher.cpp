#include "core/ExclusionMatcher.h"
#include "core/CategoryResolver.h"
#include "core/TextUtil.h"

#include <regex>
#include <cstdint>
#include <string_view>
#include <vector>

namespace pcat {
namespace {
std::vector<std::uint32_t> codePoints(std::string_view text) {
    std::vector<std::uint32_t> result;
    for (std::size_t pos=0;pos<text.size();) {
        const auto first=static_cast<unsigned char>(text[pos++]);
        if(first<0x80){result.push_back(first);continue;}
        int count=0;std::uint32_t cp=0;
        if((first&0xE0)==0xC0){count=1;cp=first&0x1F;}
        else if((first&0xF0)==0xE0){count=2;cp=first&0x0F;}
        else if((first&0xF8)==0xF0){count=3;cp=first&0x07;}
        else{result.push_back(0xFFFD);continue;}
        bool valid=true;
        for(int i=0;i<count;++i){if(pos>=text.size()){valid=false;break;}const auto c=static_cast<unsigned char>(text[pos]);if((c&0xC0)!=0x80){valid=false;break;}++pos;cp=(cp<<6)|(c&0x3F);}
        result.push_back(valid?cp:0xFFFD);
    }
    return result;
}

bool wildcardMatchFolded(std::string_view foldedPattern,std::string_view foldedText) {
    const auto pattern=codePoints(foldedPattern);
    const auto text=codePoints(foldedText);
    std::size_t p=0,t=0,star=std::string::npos,match=0;
    while(t<text.size()){
        if(p<pattern.size()&&(pattern[p]=='?'||pattern[p]==text[t])){++p;++t;continue;}
        if(p<pattern.size()&&pattern[p]=='*'){star=p++;match=t;continue;}
        if(star!=std::string::npos){p=star+1;t=++match;continue;}
        return false;
    }
    while(p<pattern.size()&&pattern[p]=='*')++p;
    return p==pattern.size();
}
}

bool isBrowserProcess(const std::string& processName) {
    const auto id = textutil::canonicalApplicationId(processName);
    return id == "chrome" || id == "google-chrome" || id == "google chrome" || id == "chromium" || id == "chromium-browser" ||
           id == "msedge" || id == "microsoft-edge" || id == "microsoft edge" || id == "firefox" || id == "opera" ||
           id == "brave" || id == "brave-browser" || id == "brave browser" || id == "vivaldi" || id == "safari";
}

std::string normalizeDomain(std::string domain) {
    domain = textutil::utf8CaseFold(textutil::trim(domain));
    const auto scheme = domain.find("://");
    if (scheme != std::string::npos) domain = domain.substr(scheme + 3);
    const auto slash = domain.find('/');
    if (slash != std::string::npos) domain.resize(slash);
    const auto query = domain.find('?');
    if (query != std::string::npos) domain.resize(query);
    const auto hash = domain.find('#');
    if (hash != std::string::npos) domain.resize(hash);
    if (const auto at=domain.rfind('@');at!=std::string::npos) domain.erase(0,at+1);
    if (!domain.empty() && domain.front() == '[') { // IPv6 literal: keep the address and discard an optional port.
        const auto close = domain.find(']');
        if (close != std::string::npos) domain.resize(close + 1);
    } else {
        const auto colon = domain.find(':');
        if (colon != std::string::npos) domain.resize(colon);
    }
    if (domain.rfind("www.", 0) == 0) domain = domain.substr(4);
    while (!domain.empty() && domain.back() == '.') domain.pop_back();
    return domain;
}

bool isExcluded(const ActivitySnapshot& snapshot, const AppSettings& settings) {
    const auto processId = textutil::canonicalApplicationId(snapshot.processName);
    for (const auto& rule : settings.excludedProcesses) {
        if (!rule.empty() && processId == textutil::canonicalApplicationId(rule)) return true;
    }
    for (const auto& title : settings.excludedWindowTitles) {
        if (textutil::equalsCaseInsensitive(snapshot.windowTitle, title)) return true;
    }
    for (const auto& pattern : settings.excludedWindowTitlePatterns) {
        try {
            if (pattern.rfind("regex:", 0) == 0) {
                // std::regex::icase is locale/implementation dependent for non-ASCII text, but explicit
                // regex syntax must not be case-folded because constructs such as \D/\S are case-sensitive.
                std::regex re(pattern.substr(6), std::regex::icase);
                if (std::regex_match(snapshot.windowTitle, re)) return true;
            } else {
                // Wildcards are user-facing text rules. Fold both sides first so Cyrillic/Latin accents
                // supported by TextUtil behave case-insensitively instead of relying on byte-wise icase.
                const auto foldedPattern = textutil::utf8CaseFold(pattern);
                const auto foldedTitle = textutil::utf8CaseFold(snapshot.windowTitle);
                if (wildcardMatchFolded(foldedPattern, foldedTitle)) return true;
            }
        } catch (...) {
            // Invalid user regex is ignored; it must not stop activity tracking.
        }
    }
    const auto domain = normalizeDomain(snapshot.browserDomain);
    for (auto rule : settings.excludedBrowserDomains) {
        rule = normalizeDomain(rule);
        if (rule.rfind("*.", 0) == 0) rule = rule.substr(2);
        if (!rule.empty() && (domain == rule || (domain.size() > rule.size() && domain.ends_with("." + rule)))) return true;
    }
    return false;
}
}
