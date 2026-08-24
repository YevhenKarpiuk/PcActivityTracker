#pragma once
#include <map>
#include <string>

namespace pcat {
std::string normalizeProcessName(std::string value);
std::string resolveCategory(const std::string& processName,
                            const std::map<std::string,std::string,std::less<>>& categories);
}
