#include "platform/Autostart.h"
#ifdef _WIN32
// libstdc++ на MinGW уже задаёт NOMINMAX=1 из bits/os_defines.h, поэтому определять его
// безусловно нельзя: получается конфликтующее переопределение.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "core/AutostartCommand.h"

#include <optional>

namespace pcat {
namespace {
constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
// Сюда Диспетчер задач (вкладка «Автозагрузка») и «Параметры → Приложения → Автозагрузка»
// пишут флаг «отключено». Запись в Run при этом остаётся, но Windows её не запускает.
constexpr const wchar_t* kApprovedKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr const wchar_t* kValueName = L"PcActivityTracker";

class RegKey {
public:
    RegKey() = default;
    RegKey(const RegKey&) = delete;
    RegKey& operator=(const RegKey&) = delete;
    ~RegKey() { if (handle) RegCloseKey(handle); }
    HKEY handle{};
};

std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string narrow(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string codeText(LONG code) { return " (код " + std::to_string(code) + ")"; }

// std::nullopt — значения нет или его нельзя прочитать.
std::optional<std::wstring> readString(HKEY key, const wchar_t* name) {
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) return std::nullopt;
    if (type != REG_SZ && type != REG_EXPAND_SZ) return std::wstring{};
    std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(buffer.data()), &size) != ERROR_SUCCESS) return std::nullopt;
    buffer.resize(wcsnlen(buffer.c_str(), buffer.size()));
    if (type == REG_EXPAND_SZ) {
        const DWORD needed = ExpandEnvironmentStringsW(buffer.c_str(), nullptr, 0);
        if (needed > 0) {
            std::wstring expanded(needed, L'\0');
            if (ExpandEnvironmentStringsW(buffer.c_str(), expanded.data(), needed) > 0) {
                expanded.resize(wcsnlen(expanded.c_str(), expanded.size()));
                return expanded;
            }
        }
    }
    return buffer;
}

bool samePath(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    if (const bool same = std::filesystem::equivalent(a, b, ec); !ec) return same;
    const auto left = a.lexically_normal().wstring(), right = b.lexically_normal().wstring();
    return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

// Запись указывает на другую существующую программу (другую копию трекера или старую .NET-версию).
// Мусорную или «мёртвую» запись, чей exe уже удалён, своей не считаем и чистим.
bool belongsToAnotherProgram(const std::wstring& command, const std::filesystem::path& self) {
    const auto exe = executableFromCommandLine(narrow(command));
    if (exe.empty()) return false;
    const std::filesystem::path target(widen(exe));
    std::error_code ec;
    if (!std::filesystem::exists(target, ec)) return false;
    return !samePath(target, self);
}

bool disabledInTaskManager() {
    RegKey key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kApprovedKey, 0, KEY_QUERY_VALUE, &key.handle) != ERROR_SUCCESS) return false;
    BYTE data[12]{};
    DWORD type = 0, size = sizeof(data);
    if (RegQueryValueExW(key.handle, kValueName, nullptr, &type, data, &size) != ERROR_SUCCESS) return false;
    // 02 00 .. — включено, 03 00 .. + время отключения — выключено. Признак — младший бит.
    return type == REG_BINARY && size >= 1 && (data[0] & 0x01) != 0;
}

LONG markEnabledInTaskManager() {
    RegKey key;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, kApprovedKey, 0, KEY_SET_VALUE, &key.handle);
    if (rc == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (rc != ERROR_SUCCESS) return rc;
    BYTE data[12]{0x02};
    return RegSetValueExW(key.handle, kValueName, 0, REG_BINARY, data, sizeof(data));
}

void forgetTaskManagerFlag() {
    RegKey key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kApprovedKey, 0, KEY_SET_VALUE, &key.handle) == ERROR_SUCCESS)
        RegDeleteValueW(key.handle, kValueName);
}

std::string utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}
} // namespace

AutostartResult setAutostart(bool enabled, const std::filesystem::path& executablePath, AutostartReason reason) {
    AutostartResult result;
    RegKey run;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &run.handle, nullptr);
    if (rc != ERROR_SUCCESS) return {false, "не удалось открыть HKCU\\...\\CurrentVersion\\Run" + codeText(rc), {}};

    const auto current = readString(run.handle, kValueName);
    if (!enabled) {
        if (!current || belongsToAnotherProgram(*current, executablePath)) return result;
        rc = RegDeleteValueW(run.handle, kValueName);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) return {false, "не удалось удалить запись автозапуска" + codeText(rc), {}};
        forgetTaskManagerFlag();
        return result;
    }

    const std::wstring command = widen(autostartCommandLine(utf8(executablePath)));
    if (command.empty()) return {false, "не удалось сформировать командную строку автозапуска", {}};
    if (!current || *current != command) {
        rc = RegSetValueExW(run.handle, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        if (rc != ERROR_SUCCESS) return {false, "не удалось записать автозапуск в реестр" + codeText(rc), {}};
    }

    if (disabledInTaskManager()) {
        if (reason == AutostartReason::UserChoice) {
            // Пользователь только что явно включил галочку — это сильнее старого запрета.
            if (rc = markEnabledInTaskManager(); rc != ERROR_SUCCESS)
                result.notice = "запись создана, но Windows держит её отключённой в Диспетчере задач (вкладка «Автозагрузка»)" + codeText(rc);
        } else {
            // При тихом старте решение пользователя из Диспетчера задач не переписываем, только сообщаем.
            result.notice = "отключён в Диспетчере задач (вкладка «Автозагрузка»). Включите его там или заново сохраните настройки";
        }
    }
    return result;
}
} // namespace pcat
#endif
