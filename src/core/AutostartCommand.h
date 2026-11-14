#pragma once
// Чистая (без OS API) логика командной строки автозапуска: её разделяют platform-бэкенды и тесты.
#include <string>
#include <string_view>

namespace pcat {
// Аргумент, с которым система запускает программу из автозагрузки. Получив его, приложение
// сразу уходит в трей и не показывает окно поверх рабочего стола при каждом входе в систему.
inline constexpr std::string_view kAutostartArgument = "--autostart";

// Командная строка для записи автозагрузки: путь всегда в кавычках (в нём бывают пробелы).
std::string autostartCommandLine(std::string_view executableUtf8);

// Исполняемый файл из командной строки автозагрузки: "C:\a b\x.exe" --flag, C:\x.exe --flag
// или старая форма .NET-версии "C:\x.exe". Пустая строка, если путь извлечь нельзя.
std::string executableFromCommandLine(std::string_view commandLine);

// Был ли процесс запущен из автозагрузки.
bool hasAutostartArgument(int argc, const char* const* argv);
}
