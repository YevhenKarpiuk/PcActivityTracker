#pragma once
#include "core/AppSettings.h"
#include <filesystem>
namespace pcat {
class SettingsService {
public:
    explicit SettingsService(std::filesystem::path path);
    AppSettings load();
    void save(const AppSettings& settings);
    const std::filesystem::path& path() const{return path_;}
private: std::filesystem::path path_;
};
}
