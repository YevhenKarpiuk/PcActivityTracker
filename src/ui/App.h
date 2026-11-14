#pragma once
#include <filesystem>
namespace pcat {
struct AppLaunchOptions {
    // Запуск из автозагрузки: окно не показывается, программа сразу работает в трее.
    bool startHidden{false};
};
int runApp(const std::filesystem::path& executablePath,const AppLaunchOptions& options={});
}
