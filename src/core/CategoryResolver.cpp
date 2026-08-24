#include "core/CategoryResolver.h"
#include "core/TextUtil.h"

namespace pcat {
std::string normalizeProcessName(std::string value) {
    return textutil::utf8CaseFold(textutil::fileNameCrossPlatform(value));
}

std::string resolveCategory(const std::string& processName,
                            const std::map<std::string,std::string,std::less<>>& categories) {
    const auto key = textutil::canonicalApplicationId(processName);
    for (const auto& [candidate, category] : categories) {
        if (textutil::canonicalApplicationId(candidate) == key) return category;
    }
    return "Без категории";
}
}
