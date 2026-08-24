#include "core/BrowserAddress.h"
#include "core/TextUtil.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace pcat {
namespace {
std::string asciiLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    return value;
}

bool validPort(std::string_view port) {
    if (port.empty() || port.size() > 5) return false;
    unsigned value = 0;
    for (const char raw : port) {
        const auto c = static_cast<unsigned char>(raw);
        if (!std::isdigit(c)) return false;
        value = value * 10U + static_cast<unsigned>(c - '0');
        if (value > 65535U) return false;
    }
    return true;
}

bool validHost(std::string_view host) {
    if (host.empty()) return false;
    if (host.front() == '[') return host.size() >= 3 && host.back() == ']';
    if (host.front() == '.' || host.find("..") != std::string_view::npos) return false;
    for (const char raw : host) {
        const auto c = static_cast<unsigned char>(raw);
        if (c <= 0x20 || c == '/' || c == '\\' || c == '?' || c == '#' || c == '@' || c == ':' || c == '[' || c == ']') return false;
    }
    return true;
}

std::size_t findAsciiInsensitive(std::string_view text, std::string_view needle) {
    if (needle.empty() || needle.size() > text.size()) return std::string_view::npos;
    for (std::size_t start = 0; start + needle.size() <= text.size(); ++start) {
        bool equal = true;
        for (std::size_t index = 0; index < needle.size(); ++index) {
            auto c = static_cast<unsigned char>(text[start + index]);
            if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + ('a' - 'A'));
            if (c != static_cast<unsigned char>(needle[index])) { equal = false; break; }
        }
        if (equal) return start;
    }
    return std::string_view::npos;
}
} // namespace

std::optional<BrowserAddress> parseBrowserAddress(std::string value) {
    value = textutil::trim(value);
    if (value.empty() || value.find_first_of(" \t\r\n") != std::string::npos) return std::nullopt;

    const auto initialScheme = value.find("://");
    if (initialScheme == std::string::npos) {
        if (value.find('.') == std::string::npos || value.find('\\') != std::string::npos) return std::nullopt;
        value = "https://" + value;
    }

    const auto schemeEnd = value.find("://");
    if (schemeEnd == std::string::npos) return std::nullopt;
    const auto scheme = asciiLower(value.substr(0, schemeEnd));
    if (scheme != "http" && scheme != "https") return std::nullopt;

    const auto authorityStart = schemeEnd + 3;
    const auto authorityEnd = value.find_first_of("/?#", authorityStart);
    auto authority = value.substr(authorityStart,
                                  authorityEnd == std::string::npos ? std::string::npos : authorityEnd - authorityStart);
    if (const auto at = authority.rfind('@'); at != std::string::npos) authority.erase(0, at + 1);
    if (authority.empty()) return std::nullopt;

    std::string host;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos) return std::nullopt;
        host = authority.substr(0, close + 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':' || !validPort(std::string_view(authority).substr(close + 2))) return std::nullopt;
        }
    } else {
        const auto colon = authority.find(':');
        if (colon != std::string::npos) {
            if (authority.find(':', colon + 1) != std::string::npos) return std::nullopt; // IPv6 literals require brackets.
            if (!validPort(std::string_view(authority).substr(colon + 1))) return std::nullopt;
        }
        host = authority.substr(0, colon);
    }
    host = asciiLower(textutil::trim(host));
    while (!host.empty() && host.back() == '.') host.pop_back();
    if (!validHost(host)) return std::nullopt;
    return BrowserAddress{std::move(value), std::move(host)};
}

std::optional<BrowserAddress> findBrowserAddressInText(std::string_view text) {
    auto https = findAsciiInsensitive(text, "https://");
    auto http = findAsciiInsensitive(text, "http://");
    std::size_t start = std::string_view::npos;
    if (https != std::string_view::npos && http != std::string_view::npos) start = std::min(https, http);
    else start = https != std::string_view::npos ? https : http;
    if (start == std::string_view::npos) return std::nullopt;
    const auto end = text.find_first_of(" \t\r\n\"'<>()", start);
    return parseBrowserAddress(std::string(text.substr(start, end == std::string::npos ? std::string::npos : end - start)));
}
} // namespace pcat
