#include "core/CategoryResolver.h"
#include "core/TextUtil.h"

namespace pcat {
namespace {
constexpr const char* kNoCategory = "Без категории";
}

std::string normalizeProcessName(std::string value) {
    return textutil::utf8CaseFold(textutil::fileNameCrossPlatform(value));
}

CategoryIndex buildCategoryIndex(const CategoryMap& categories) {
    CategoryIndex index;
    for (const auto& [candidate, category] : categories) {
        auto key = textutil::canonicalApplicationId(candidate);
        if (key.empty()) continue;
        // Первое совпадение выигрывает, как и в прежнем линейном поиске по упорядоченной карте.
        index.byApplicationId.emplace(std::move(key), category);
    }
    return index;
}

std::string resolveCategory(const std::string& processName, const CategoryIndex& index) {
    const auto key = textutil::canonicalApplicationId(processName);
    const auto found = index.byApplicationId.find(key);
    return found != index.byApplicationId.end() ? found->second : kNoCategory;
}

std::string resolveCategory(const std::string& processName, const CategoryMap& categories) {
    return resolveCategory(processName, buildCategoryIndex(categories));
}
}
