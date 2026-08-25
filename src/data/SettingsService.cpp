#include "data/SettingsService.h"
#include "core/ExclusionMatcher.h"
#include "core/TextUtil.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cmath>
#include <limits>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace pcat {
namespace {
struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue, std::less<>>;
struct JsonValue {
    std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject> value{nullptr};
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}
    JsonValue parse() {
        skipWs();
        auto v = parseValue();
        skipWs();
        if (pos_ != text_.size()) fail("unexpected trailing JSON data");
        return v;
    }
private:
    [[noreturn]] void fail(const char* what) const { throw std::runtime_error(std::string("settings.json: ") + what); }
    void skipWs() { while (pos_ < text_.size() && (text_[pos_]==' ' || text_[pos_]=='\t' || text_[pos_]=='\r' || text_[pos_]=='\n')) ++pos_; }
    bool consume(char c) { skipWs(); if (pos_ < text_.size() && text_[pos_] == c) { ++pos_; return true; } return false; }
    void expect(char c) { if (!consume(c)) fail("unexpected token"); }

    static void appendUtf8(std::string& out, std::uint32_t cp) {
        if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::uint32_t hex4() {
        if (pos_ + 4 > text_.size()) fail("truncated unicode escape");
        std::uint32_t result = 0;
        for (int i=0;i<4;++i) {
            const char c=text_[pos_++];
            result <<= 4;
            if (c>='0'&&c<='9') result += static_cast<std::uint32_t>(c-'0');
            else if (c>='a'&&c<='f') result += static_cast<std::uint32_t>(10+c-'a');
            else if (c>='A'&&c<='F') result += static_cast<std::uint32_t>(10+c-'A');
            else fail("invalid unicode escape");
        }
        return result;
    }

    std::string parseString() {
        skipWs();
        if (pos_ >= text_.size() || text_[pos_] != '"') fail("expected string");
        ++pos_;
        std::string out;
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_++]);
            if (c == '"') return out;
            if (c < 0x20) fail("control character in string");
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
            if (pos_ >= text_.size()) fail("truncated escape");
            const char e = text_[pos_++];
            switch(e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 2 > text_.size() || text_[pos_] != '\\' || text_[pos_+1] != 'u') fail("missing low surrogate");
                        pos_ += 2;
                        const auto low = hex4();
                        if (low < 0xDC00 || low > 0xDFFF) fail("invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) fail("unexpected low surrogate");
                    appendUtf8(out, cp);
                    break;
                }
                default: fail("invalid escape");
            }
        }
        fail("unterminated string");
    }

    JsonValue parseNumber() {
        skipWs();
        const auto start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;

        // JSON numbers do not allow an empty integer part or leading zeroes such as 01.
        if (pos_ >= text_.size()) fail("invalid number");
        if (text_[pos_] == '0') {
            ++pos_;
            if (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') fail("invalid number");
        } else if (text_[pos_] >= '1' && text_[pos_] <= '9') {
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        } else {
            fail("invalid number");
        }

        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            const auto fractionStart = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            if (fractionStart == pos_) fail("invalid number");
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            const auto exponentStart = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            if (exponentStart == pos_) fail("invalid number");
        }
        try { return JsonValue{std::stod(std::string(text_.substr(start, pos_ - start)))}; }
        catch (...) { fail("invalid number"); }
    }

    JsonValue parseArray() {
        expect('['); JsonArray a; skipWs(); if (consume(']')) return JsonValue{std::move(a)};
        while (true) { a.push_back(parseValue()); if (consume(']')) break; expect(','); }
        return JsonValue{std::move(a)};
    }
    JsonValue parseObject() {
        expect('{'); JsonObject o; skipWs(); if (consume('}')) return JsonValue{std::move(o)};
        while (true) { auto key = parseString(); expect(':'); o[std::move(key)] = parseValue(); if (consume('}')) break; expect(','); }
        return JsonValue{std::move(o)};
    }
    JsonValue parseValue() {
        skipWs(); if (pos_ >= text_.size()) fail("unexpected end of file");
        switch(text_[pos_]) {
            case '"': return JsonValue{parseString()};
            case '{': return parseObject();
            case '[': return parseArray();
            case 't': if (text_.substr(pos_,4)=="true") {pos_+=4;return JsonValue{true};} break;
            case 'f': if (text_.substr(pos_,5)=="false") {pos_+=5;return JsonValue{false};} break;
            case 'n': if (text_.substr(pos_,4)=="null") {pos_+=4;return JsonValue{nullptr};} break;
            default: if (text_[pos_]=='-' || (text_[pos_]>='0'&&text_[pos_]<='9')) return parseNumber();
        }
        fail("invalid value");
    }
    std::string_view text_;
    std::size_t pos_{};
};

std::string readAll(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot read settings.json");
    std::ostringstream buffer;
    buffer << f.rdbuf();
    if (f.bad()) throw std::runtime_error("Failed while reading settings.json");
    return buffer.str();
}

const JsonObject* asObject(const JsonValue& value) { return std::get_if<JsonObject>(&value.value); }
const JsonArray* asArray(const JsonValue& value) { return std::get_if<JsonArray>(&value.value); }
const std::string* asString(const JsonValue& value) { return std::get_if<std::string>(&value.value); }
const bool* asBool(const JsonValue& value) { return std::get_if<bool>(&value.value); }
const double* asNumber(const JsonValue& value) { return std::get_if<double>(&value.value); }
const JsonValue* find(const JsonObject& o, std::initializer_list<const char*> keys) {
    for (const auto* key : keys) { auto it=o.find(key); if (it!=o.end()) return &it->second; }
    return nullptr;
}
// Ошибка типа в одном поле — это не повреждение файла. Такое поле пропускается со
// значением по умолчанию, а не приводит к перезаписи всех настроек пользователя.
void warnField(std::vector<std::string>& warnings, std::initializer_list<const char*> keys, const char* expected) {
    warnings.push_back(std::string(*keys.begin()) + ": ожидалось " + expected + ", значение проигнорировано");
}

std::optional<int> getInt(const JsonObject& o, std::initializer_list<const char*> keys, std::vector<std::string>& warnings) {
    const auto* v=find(o,keys);
    if (!v) return std::nullopt;
    const auto* n=asNumber(*v);
    if (!n || !std::isfinite(*n) || std::trunc(*n) != *n
        || *n < static_cast<double>(std::numeric_limits<int>::min())
        || *n > static_cast<double>(std::numeric_limits<int>::max())) {
        warnField(warnings, keys, "целое число");
        return std::nullopt;
    }
    return static_cast<int>(*n);
}
std::optional<bool> getBool(const JsonObject& o, std::initializer_list<const char*> keys, std::vector<std::string>& warnings) {
    const auto* v=find(o,keys);
    if (!v) return std::nullopt;
    const auto* b=asBool(*v);
    if (!b) { warnField(warnings, keys, "true или false"); return std::nullopt; }
    return *b;
}
std::optional<std::vector<std::string>> getStringArray(const JsonObject& o, std::initializer_list<const char*> keys, std::vector<std::string>& warnings) {
    const auto* v=find(o,keys); if (!v) return std::nullopt;
    const auto* a=asArray(*v);
    if (!a) { warnField(warnings, keys, "массив строк"); return std::nullopt; }
    std::vector<std::string> result;
    result.reserve(a->size());
    for (const auto& item:*a) {
        const auto* s=asString(item);
        if (!s) { warnField(warnings, keys, "массив строк без посторонних значений"); return std::nullopt; }
        result.push_back(*s);
    }
    return result;
}
std::optional<std::map<std::string,std::string,std::less<>>> getStringMap(const JsonObject& o, std::initializer_list<const char*> keys, std::vector<std::string>& warnings) {
    const auto* v=find(o,keys); if (!v) return std::nullopt;
    const auto* m=asObject(*v);
    if (!m) { warnField(warnings, keys, "объект вида приложение-категория"); return std::nullopt; }
    std::map<std::string,std::string,std::less<>> result;
    for (const auto& [k,item]:*m) {
        const auto* s=asString(item);
        if (!s) { warnField(warnings, keys, "строковые значения категорий"); return std::nullopt; }
        result[k]=*s;
    }
    return result;
}

std::string escapeJson(const std::string& s) {
    static constexpr char hex[]="0123456789ABCDEF";
    std::string out;
    for (const char raw:s) {
        const auto c=static_cast<unsigned char>(raw);
        switch(c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { out += "\\u00"; out += hex[c>>4]; out += hex[c&15]; }
                else out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

void normalize(AppSettings& s) {
    s.idleThresholdSeconds=std::clamp(s.idleThresholdSeconds,10,86400);
    s.pollingIntervalSeconds=std::clamp(s.pollingIntervalSeconds,1,60);

    std::set<std::string> seen;
    std::vector<std::string> processes;
    for (const auto& raw:s.excludedProcesses) {
        auto rule=textutil::trim(raw); if(rule.empty()) continue;
        const auto id=textutil::canonicalApplicationId(rule); if(id.empty()||!seen.insert(id).second) continue;
        processes.push_back(std::move(rule));
    }
    s.excludedProcesses=std::move(processes);

    std::vector<std::string> domains;
    for(auto domain:s.excludedBrowserDomains){domain=normalizeDomain(domain);if(!domain.empty())domains.push_back(std::move(domain));}
    std::sort(domains.begin(),domains.end());domains.erase(std::unique(domains.begin(),domains.end()),domains.end());s.excludedBrowserDomains=std::move(domains);

    for (auto it=s.categories.begin();it!=s.categories.end();) {
        if (textutil::canonicalApplicationId(it->first).empty() || textutil::trim(it->second).empty()) it=s.categories.erase(it);
        else ++it;
    }
}

void writeArray(std::ostream& o,const char* name,const std::vector<std::string>& a,bool comma=true) {
    o<<"  \""<<name<<"\": [";
    for(std::size_t i=0;i<a.size();++i){if(i)o<<", ";o<<'"'<<escapeJson(a[i])<<'"';}
    o<<']'<<(comma?",":"")<<'\n';
}

void recoverBackupIfNeeded(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return;
    auto backup=path; backup += ".bak";
    if (!std::filesystem::exists(backup)) return;
    std::error_code ec; std::filesystem::rename(backup,path,ec);
    if (ec) throw std::runtime_error("Cannot recover settings backup: "+ec.message());
}
}

SettingsService::SettingsService(std::filesystem::path path):path_(std::move(path)){}

AppSettings SettingsService::load() {
    AppSettings settings;
    warnings_.clear();
    recoverBackupIfNeeded(path_);
    if(!std::filesystem::exists(path_)){save(settings);return settings;}
    if(!std::filesystem::is_regular_file(path_))throw std::runtime_error("settings.json is not a regular file");
    // I/O failures are not proof of corruption. Never replace a valid settings file with defaults merely
    // because it is temporarily unreadable (permissions, antivirus, filesystem error, etc.).
    auto text=readAll(path_);
    try {
        if(text.empty()) throw std::runtime_error("empty settings");
        if(text.size()>=3&&static_cast<unsigned char>(text[0])==0xEF&&static_cast<unsigned char>(text[1])==0xBB&&static_cast<unsigned char>(text[2])==0xBF)text.erase(0,3);
        const auto root=JsonParser(text).parse(); const auto* object=asObject(root); if(!object) throw std::runtime_error("settings root is not an object");
        if(auto v=getInt(*object,{"IdleThresholdSeconds","idleThresholdSeconds"},warnings_))settings.idleThresholdSeconds=*v;
        if(auto v=getInt(*object,{"PollingIntervalSeconds","pollingIntervalSeconds"},warnings_))settings.pollingIntervalSeconds=*v;
        if(auto v=getBool(*object,{"HideBrowserWindowTitles","hideBrowserWindowTitles"},warnings_))settings.hideBrowserWindowTitles=*v;
        if(auto v=getBool(*object,{"StartWithSystem","StartWithWindows","startWithSystem"},warnings_))settings.startWithSystem=*v;
        if(auto a=getStringArray(*object,{"ExcludedProcesses","excludedProcesses"},warnings_))settings.excludedProcesses=std::move(*a);
        if(auto a=getStringArray(*object,{"ExcludedWindowTitles","excludedWindowTitles"},warnings_))settings.excludedWindowTitles=std::move(*a);
        if(auto a=getStringArray(*object,{"ExcludedWindowTitlePatterns","excludedWindowTitlePatterns"},warnings_))settings.excludedWindowTitlePatterns=std::move(*a);
        if(auto a=getStringArray(*object,{"ExcludedBrowserDomains","excludedBrowserDomains"},warnings_))settings.excludedBrowserDomains=std::move(*a);
        if(auto m=getStringMap(*object,{"Categories","categories"},warnings_))settings.categories=std::move(*m);
        normalize(settings); return settings;
    } catch (...) {
        try { auto corrupt=path_;corrupt += ".corrupt";std::filesystem::copy_file(path_,corrupt,std::filesystem::copy_options::overwrite_existing); } catch (...) {}
        save(settings); return settings;
    }
}

void SettingsService::save(const AppSettings& source) {
    AppSettings settings=source; normalize(settings);
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
    if (std::filesystem::exists(path_) && !std::filesystem::is_regular_file(path_))
        throw std::runtime_error("settings.json is not a regular file");
    auto temp=path_;temp += ".tmp"; auto backup=path_;backup += ".bak";
    {
        std::ofstream o(temp,std::ios::binary|std::ios::trunc); if(!o) throw std::runtime_error("Cannot write settings.json.tmp");
        o<<"{\n";
        o<<"  \"IdleThresholdSeconds\": "<<settings.idleThresholdSeconds<<",\n";
        o<<"  \"PollingIntervalSeconds\": "<<settings.pollingIntervalSeconds<<",\n";
        o<<"  \"HideBrowserWindowTitles\": "<<(settings.hideBrowserWindowTitles?"true":"false")<<",\n";
        o<<"  \"StartWithSystem\": "<<(settings.startWithSystem?"true":"false")<<",\n";
        o<<"  \"StartWithWindows\": "<<(settings.startWithSystem?"true":"false")<<",\n";
        writeArray(o,"ExcludedProcesses",settings.excludedProcesses);
        writeArray(o,"ExcludedWindowTitles",settings.excludedWindowTitles);
        writeArray(o,"ExcludedWindowTitlePatterns",settings.excludedWindowTitlePatterns);
        writeArray(o,"ExcludedBrowserDomains",settings.excludedBrowserDomains);
        o<<"  \"Categories\": {\n";
        std::size_t i=0;for(const auto&[k,v]:settings.categories)o<<"    \""<<escapeJson(k)<<"\": \""<<escapeJson(v)<<"\""<<(++i<settings.categories.size()?",":"")<<"\n";
        o<<"  }\n}\n";
        o.flush(); if(!o) throw std::runtime_error("Failed to flush settings.json.tmp");
    }

    std::error_code ec; std::filesystem::remove(backup,ec); ec.clear();
    if(std::filesystem::exists(path_)){std::filesystem::rename(path_,backup,ec);if(ec){std::filesystem::remove(temp);throw std::runtime_error("Cannot create settings backup: "+ec.message());}}
    ec.clear();std::filesystem::rename(temp,path_,ec);
    if(ec){std::error_code restore;if(std::filesystem::exists(backup))std::filesystem::rename(backup,path_,restore);std::filesystem::remove(temp);throw std::runtime_error("Cannot replace settings.json: "+ec.message());}
    std::filesystem::remove(backup,ec);
}
}
