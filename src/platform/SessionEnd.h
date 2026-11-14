#pragma once
#include <functional>
#include <memory>

namespace pcat {
// Сообщает о завершении сеанса ОС (выход из системы, перезагрузка, выключение).
// На Windows SDL3 не обрабатывает WM_ENDSESSION: после ответа на него процесс могут убить
// в любой момент, и текущий интервал активности не успел бы записаться. Обработчик
// вызывается синхронно в потоке окна, до того как Windows продолжит завершение.
// nativeWindow — HWND главного окна на Windows; на других платформах игнорируется
// (там SDL сам превращает SIGTERM в SDL_EVENT_QUIT).
class SessionEndWatcher {
public:
    SessionEndWatcher(void* nativeWindow, std::function<void()> onSessionEnd);
    ~SessionEndWatcher();
    SessionEndWatcher(const SessionEndWatcher&) = delete;
    SessionEndWatcher& operator=(const SessionEndWatcher&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
