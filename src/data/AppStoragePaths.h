#pragma once
#include <filesystem>
namespace pcat { std::filesystem::path appDataDirectory(const std::filesystem::path& executableDirectory = std::filesystem::current_path()); }
