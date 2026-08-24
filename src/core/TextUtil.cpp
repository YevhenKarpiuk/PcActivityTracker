#include "core/TextUtil.h"

#include <cstdint>

namespace pcat::textutil {
namespace {
std::uint32_t decodeOne(std::string_view text, std::size_t& pos) {
    const auto first = static_cast<unsigned char>(text[pos++]);
    if (first < 0x80) return first;
    int count = 0;
    std::uint32_t cp = 0;
    if ((first & 0xE0) == 0xC0) { count = 1; cp = first & 0x1F; }
    else if ((first & 0xF0) == 0xE0) { count = 2; cp = first & 0x0F; }
    else if ((first & 0xF8) == 0xF0) { count = 3; cp = first & 0x07; }
    else return 0xFFFD;
    for (int i = 0; i < count; ++i) {
        if (pos >= text.size()) return 0xFFFD;
        const auto c = static_cast<unsigned char>(text[pos]);
        if ((c & 0xC0) != 0x80) return 0xFFFD;
        ++pos;
        cp = (cp << 6) | (c & 0x3F);
    }
    return cp;
}

void appendUtf8(std::string& out, std::uint32_t cp) {
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

std::uint32_t foldCodePoint(std::uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 0x20;
    if (cp >= 0x00C0 && cp <= 0x00DE && cp != 0x00D7) return cp + 0x20;
    if (cp >= 0x0391 && cp <= 0x03A1) return cp + 0x20;
    if (cp >= 0x03A3 && cp <= 0x03AB) return cp + 0x20;
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;
    if (cp >= 0x0400 && cp <= 0x040F) return cp + 0x50; // Ё, Є, І, Ї and related Cyrillic letters.
    if (cp == 0x0490) return 0x0491;                   // Ґ -> ґ
    if (cp == 0x1E9E) return 0x00DF;                   // ẞ -> ß
    return cp;
}
}

std::string trim(std::string_view text) {
    std::size_t first = 0;
    while (first < text.size()) {
        const auto c = static_cast<unsigned char>(text[first]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        ++first;
    }
    std::size_t last = text.size();
    while (last > first) {
        const auto c = static_cast<unsigned char>(text[last - 1]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        --last;
    }
    return std::string(text.substr(first, last - first));
}

std::string utf8CaseFold(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) appendUtf8(out, foldCodePoint(decodeOne(text, pos)));
    return out;
}

bool equalsCaseInsensitive(std::string_view left, std::string_view right) {
    return utf8CaseFold(left) == utf8CaseFold(right);
}

bool containsCaseInsensitive(std::string_view text, std::string_view needle) {
    if (needle.empty()) return true;
    return utf8CaseFold(text).find(utf8CaseFold(needle)) != std::string::npos;
}

std::string fileNameCrossPlatform(std::string_view pathOrName) {
    auto value = trim(pathOrName);
    const auto pos = value.find_last_of("/\\");
    if (pos != std::string::npos) value.erase(0, pos + 1);
    return value;
}

std::string canonicalApplicationId(std::string_view pathOrName) {
    auto value = utf8CaseFold(fileNameCrossPlatform(pathOrName));
    if (value.size() > 4 && value.ends_with(".exe")) value.resize(value.size() - 4);
    return value;
}
}
