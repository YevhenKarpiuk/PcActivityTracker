#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace pcat {
struct BrowserAddress {
    std::string url;
    std::string domain;
};

// Parses a browser address-bar value. Plain hosts such as example.com/path are accepted and
// normalized to https://. Only HTTP(S) addresses are returned.
std::optional<BrowserAddress> parseBrowserAddress(std::string value);
std::optional<BrowserAddress> findBrowserAddressInText(std::string_view text);
}
