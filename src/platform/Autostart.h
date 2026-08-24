#pragma once
#include <filesystem>
#include <string>
namespace pcat {
bool setAutostart(bool enabled,const std::filesystem::path& executablePath,std::string& error);
}
