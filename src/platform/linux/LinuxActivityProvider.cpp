#include "platform/Factory.h"
#include "core/BrowserAddress.h"
#include "core/ExclusionMatcher.h"

#include <memory>

#ifdef PCAT_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#ifdef PCAT_HAVE_XSS
#include <X11/extensions/scrnsaver.h>
#endif
#include <filesystem>
#include <string>
#endif

namespace pcat {
#ifdef PCAT_HAVE_X11
namespace {
std::string windowUtf8Title(Display* display, Window window) {
    const Atom netWmName = XInternAtom(display, "_NET_WM_NAME", True);
    const Atom utf8String = XInternAtom(display, "UTF8_STRING", True);
    if (netWmName != None && utf8String != None) {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(display, window, netWmName, 0, 8192, False, utf8String,
                               &actualType, &actualFormat, &itemCount, &bytesAfter, &data) == Success && data) {
            std::string result(reinterpret_cast<char*>(data), itemCount);
            XFree(data);
            if (!result.empty()) return result;
        }
        if (data) XFree(data);
    }

    char* legacyName = nullptr;
    if (XFetchName(display, window, &legacyName) && legacyName) {
        std::string result = legacyName;
        XFree(legacyName);
        return result;
    }
    return {};
}

Window activeWindow(Display* display, Window root) {
    const Atom active = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    if (active == None) return 0;
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    Window result = 0;
    if (XGetWindowProperty(display, root, active, 0, 1, False, XA_WINDOW,
                           &actualType, &actualFormat, &itemCount, &bytesAfter, &data) == Success && data) {
        result = *reinterpret_cast<Window*>(data);
    }
    if (data) XFree(data);
    return result;
}

unsigned long windowPid(Display* display, Window window) {
    const Atom pidAtom = XInternAtom(display, "_NET_WM_PID", True);
    if (pidAtom == None) return 0;
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    unsigned long pid = 0;
    if (XGetWindowProperty(display, window, pidAtom, 0, 1, False, XA_CARDINAL,
                           &actualType, &actualFormat, &itemCount, &bytesAfter, &data) == Success && data) {
        pid = *reinterpret_cast<unsigned long*>(data);
    }
    if (data) XFree(data);
    return pid;
}
} // namespace
#endif

class LinuxActivityProvider final : public IActivityProvider {
public:
    ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        snapshot.timestamp = std::chrono::system_clock::now();
#ifdef PCAT_HAVE_X11
        Display* display = XOpenDisplay(nullptr);
        if (!display) return snapshot;
        const Window root = DefaultRootWindow(display);
        const Window window = activeWindow(display, root);
        if (window) {
            snapshot.windowTitle = windowUtf8Title(display, window);
            const auto pid = windowPid(display, window);
            if (pid != 0) {
                std::error_code error;
                const auto path = std::filesystem::read_symlink("/proc/" + std::to_string(pid) + "/exe", error);
                if (!error) {
                    snapshot.exePath = path.string();
                    snapshot.processName = path.filename().string();
                }
            }
        }
#ifdef PCAT_HAVE_XSS
        XScreenSaverInfo* info = XScreenSaverAllocInfo();
        if (info && XScreenSaverQueryInfo(display, root, info)) {
            snapshot.idleSeconds = static_cast<int>(info->idle / 1000UL);
        }
        if (info) XFree(info);
#endif
        XCloseDisplay(display);
        if (isBrowserProcess(snapshot.processName)) {
            if (const auto address = findBrowserAddressInText(snapshot.windowTitle)) {
                snapshot.browserUrl = address->url;
                snapshot.browserDomain = address->domain;
            }
        }
#endif
        return snapshot;
    }

    ProviderCapabilities capabilities() const override {
#ifdef PCAT_HAVE_X11
#ifdef PCAT_HAVE_XSS
        return {true, true, true, false,
                "X11/XWayland backend. Native Wayland may restrict global active-window access; browser URL is unavailable."};
#else
        return {true, false, true, false,
                "X11 backend; XScreenSaver is unavailable, so idle time and browser URL are unavailable."};
#endif
#else
        return {false, false, false, false,
                "Built without X11. Core/database/reports work, but global activity capture is unavailable."};
#endif
    }
};

std::unique_ptr<IActivityProvider> createActivityProvider() {
    return std::make_unique<LinuxActivityProvider>();
}
} // namespace pcat
