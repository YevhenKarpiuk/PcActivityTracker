#pragma once

#include <string>
#include <string_view>

namespace pcat::textutil {
std::string trim(std::string_view text);
std::string utf8CaseFold(std::string_view text);
bool equalsCaseInsensitive(std::string_view left, std::string_view right);
bool containsCaseInsensitive(std::string_view text, std::string_view needle);
std::string fileNameCrossPlatform(std::string_view pathOrName);
std::string canonicalApplicationId(std::string_view pathOrName);
}
