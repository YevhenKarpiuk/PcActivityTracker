#include "platform/SessionEnd.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

namespace pcat {
namespace {
constexpr UINT_PTR kSubclassId = 0x50434154; // "PCAT"
}

struct SessionEndWatcher::Impl {
    HWND window{};
    std::function<void()> onSessionEnd;
    bool installed{false};
    bool fired{false};

    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        // wParam == FALSE — завершение сеанса отменено (другая программа отказалась закрываться).
        if (message == WM_ENDSESSION && wParam && self && !self->fired && self->onSessionEnd) {
            self->fired = true;
            try {
                self->onSessionEnd();
            } catch (...) {
            }
        }
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
};

SessionEndWatcher::SessionEndWatcher(void* nativeWindow, std::function<void()> onSessionEnd) : impl_(std::make_unique<Impl>()) {
    impl_->window = static_cast<HWND>(nativeWindow);
    impl_->onSessionEnd = std::move(onSessionEnd);
    if (impl_->window)
        impl_->installed = SetWindowSubclass(impl_->window, &Impl::proc, kSubclassId, reinterpret_cast<DWORD_PTR>(impl_.get())) != FALSE;
}

SessionEndWatcher::~SessionEndWatcher() {
    if (impl_->installed) RemoveWindowSubclass(impl_->window, &Impl::proc, kSubclassId);
}
} // namespace pcat
#endif
