#pragma once
#include <filesystem>
#include <string>
namespace pcat {
// Кто инициирует синхронизацию: Startup — тихая сверка при каждом запуске программы,
// UserChoice — пользователь явно нажал «Сохранить настройки».
enum class AutostartReason { Startup, UserChoice };

struct AutostartResult {
    bool ok{true};
    std::string error;   // операция не выполнена
    std::string notice;  // операция выполнена, но пользователю надо что-то знать
};

// Включает или выключает автозапуск именно этой копии программы. Выключение не трогает запись,
// которая указывает на другую существующую копию: иначе запуск dev-сборки с выключенной
// галочкой молча ломал бы автозапуск основной установки.
AutostartResult setAutostart(bool enabled,const std::filesystem::path& executablePath,AutostartReason reason);
}
