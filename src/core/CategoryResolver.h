#pragma once
#include <map>
#include <string>

namespace pcat {
using CategoryMap = std::map<std::string, std::string, std::less<>>;

// Индекс категорий с уже канонизированными ключами. Строится один раз при смене настроек,
// чтобы опрос активности не пересчитывал canonicalApplicationId для каждой записи карты.
// Отдельный тип, а не псевдоним CategoryMap: иначе сырую карту можно было бы молча
// передать в быстрый путь и получить неверную категорию.
struct CategoryIndex {
    CategoryMap byApplicationId;
};

std::string normalizeProcessName(std::string value);

CategoryIndex buildCategoryIndex(const CategoryMap& categories);

// Быстрый путь: один поиск по готовому индексу.
std::string resolveCategory(const std::string& processName, const CategoryIndex& index);

// Удобная форма для тестов и разовых вызовов: строит индекс на месте.
std::string resolveCategory(const std::string& processName, const CategoryMap& categories);
}
