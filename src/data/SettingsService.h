#pragma once
#include "core/AppSettings.h"
#include <filesystem>
#include <string>
#include <vector>
namespace pcat {
class SettingsService {
public:
    explicit SettingsService(std::filesystem::path path);
    AppSettings load();
    void save(const AppSettings& settings);
    const std::filesystem::path& path() const{return path_;}
    // Поля, которые последний load() не смог прочитать и заменил значением по умолчанию.
    // Такие ошибки больше не считаются повреждением файла, поэтому о них нужно сообщить,
    // иначе пользователь не узнает, почему одна настройка «не применилась».
    const std::vector<std::string>& lastLoadWarnings() const{return warnings_;}
private:
    std::filesystem::path path_;
    std::vector<std::string> warnings_;
};
}
