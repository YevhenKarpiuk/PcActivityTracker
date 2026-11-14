#include "platform/SingleInstance.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cwctype>
#include <string>

namespace pcat {
namespace {
// Стабильный между сборками хеш (std::hash таким не обязан): имя объекта ядра должно совпадать
// у любых двух процессов, открывших один и тот же каталог данных.
std::wstring dataDirectoryKey(const std::filesystem::path& dataDirectory) {
    std::error_code ec;
    auto absolute = std::filesystem::weakly_canonical(dataDirectory, ec);
    if (ec) absolute = dataDirectory.lexically_normal();
    std::wstring text = absolute.wstring();
    while (!text.empty() && (text.back() == L'\\' || text.back() == L'/')) text.pop_back();
    std::uint64_t hash = 1469598103934665603ULL;
    for (wchar_t c : text) {
        const auto lower = static_cast<std::uint32_t>(std::towlower(static_cast<std::wint_t>(c == L'/' ? L'\\' : c)));
        hash ^= lower;
        hash *= 1099511628211ULL;
    }
    wchar_t buffer[17]{};
    swprintf(buffer, 17, L"%016llx", static_cast<unsigned long long>(hash));
    return buffer;
}
} // namespace

struct SingleInstance::Impl {
    HANDLE mutex{};
    HANDLE showEvent{};
    bool primary{true};
    ~Impl() {
        if (showEvent) CloseHandle(showEvent);
        if (mutex) CloseHandle(mutex);
    }
};

SingleInstance::SingleInstance(const std::filesystem::path& dataDirectory) : impl_(std::make_unique<Impl>()) {
    const auto key = dataDirectoryKey(dataDirectory);
    const std::wstring mutexName = L"Local\\PcActivityTracker-" + key;
    const std::wstring eventName = L"Local\\PcActivityTracker-show-" + key;
    impl_->mutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
    // Если мьютекс создать не удалось вовсе, лучше работать, чем молча не запуститься.
    impl_->primary = !(impl_->mutex && GetLastError() == ERROR_ALREADY_EXISTS);
    impl_->showEvent = CreateEventW(nullptr, FALSE, FALSE, eventName.c_str());
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::isPrimary() const noexcept { return impl_->primary; }

void SingleInstance::requestShowPrimary() noexcept {
    if (!impl_->showEvent) return;
    // Иначе Windows не даст уже работающему процессу вывести окно на передний план.
    AllowSetForegroundWindow(ASFW_ANY);
    SetEvent(impl_->showEvent);
}

bool SingleInstance::consumeShowRequest() noexcept {
    return impl_->showEvent && WaitForSingleObject(impl_->showEvent, 0) == WAIT_OBJECT_0;
}
} // namespace pcat
#endif
