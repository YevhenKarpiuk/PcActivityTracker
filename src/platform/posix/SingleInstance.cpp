#include "platform/SingleInstance.h"
// Linux и macOS: эксклюзивная flock-блокировка файла в каталоге данных. Ядро снимает её само,
// если процесс упал, поэтому «зависшей» блокировки после сбоя не бывает.
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <chrono>

namespace pcat {
struct SingleInstance::Impl {
    int lockFd{-1};
    bool primary{true};
    std::filesystem::path showRequest;
    std::chrono::steady_clock::time_point lastCheck{};
    ~Impl() {
        if (lockFd >= 0) close(lockFd); // закрытие снимает flock
    }
};

SingleInstance::SingleInstance(const std::filesystem::path& dataDirectory) : impl_(std::make_unique<Impl>()) {
    const auto lockPath = dataDirectory / ".instance.lock";
    impl_->showRequest = dataDirectory / ".show-request";
    impl_->lockFd = ::open(lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    // Не удалось открыть файл блокировки (read-only носитель) — лучше работать, чем не запуститься.
    if (impl_->lockFd < 0) return;
    if (flock(impl_->lockFd, LOCK_EX | LOCK_NB) != 0) {
        impl_->primary = false;
        close(impl_->lockFd);
        impl_->lockFd = -1;
    } else {
        std::error_code ec;
        std::filesystem::remove(impl_->showRequest, ec); // просьба, оставшаяся от прошлого сеанса
    }
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::isPrimary() const noexcept { return impl_->primary; }

void SingleInstance::requestShowPrimary() noexcept {
    const int fd = ::open(impl_->showRequest.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if (fd >= 0) close(fd);
}

bool SingleInstance::consumeShowRequest() noexcept {
    // Вызывается каждый кадр: файловую систему достаточно трогать пару раз в секунду.
    const auto now = std::chrono::steady_clock::now();
    if (now - impl_->lastCheck < std::chrono::milliseconds(500)) return false;
    impl_->lastCheck = now;
    std::error_code ec;
    return std::filesystem::remove(impl_->showRequest, ec) && !ec;
}
} // namespace pcat
#endif
