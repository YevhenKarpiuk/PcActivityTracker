#include "platform/Factory.h"
#include "core/BrowserAddress.h"
#include "core/ExclusionMatcher.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

#include <memory>
#include <optional>
#include <string>

namespace pcat {
namespace {
std::string utf8(NSString* value) {
    if (!value) return {};
    const char* text = [value UTF8String];
    return text ? std::string(text) : std::string{};
}

std::string cfStringUtf8(CFTypeRef value) {
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return {};
    NSString* string = (__bridge NSString*)value;
    return utf8(string);
}

struct AccessibilityResult {
    std::string title;
    std::optional<BrowserAddress> browserAddress;
};

AccessibilityResult accessibilityInfo(pid_t pid, bool browser) {
    AccessibilityResult result;
    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) return result;

    CFTypeRef focusedWindow = nullptr;
    if (AXUIElementCopyAttributeValue(app, kAXFocusedWindowAttribute, &focusedWindow) == kAXErrorSuccess && focusedWindow) {
        CFTypeRef title = nullptr;
        if (AXUIElementCopyAttributeValue((AXUIElementRef)focusedWindow, kAXTitleAttribute, &title) == kAXErrorSuccess && title) {
            result.title = cfStringUtf8(title);
            CFRelease(title);
        }

        if (browser) {
            // Safari/Chromium-based browsers commonly expose the current document URL through the
            // accessibility document attribute. When unavailable, the caller falls back gracefully.
            CFTypeRef document = nullptr;
            if (AXUIElementCopyAttributeValue((AXUIElementRef)focusedWindow, kAXDocumentAttribute, &document) == kAXErrorSuccess && document) {
                result.browserAddress = parseBrowserAddress(cfStringUtf8(document));
                CFRelease(document);
            }
        }
        CFRelease(focusedWindow);
    }
    CFRelease(app);
    return result;
}
} // namespace

class MacActivityProvider final : public IActivityProvider {
public:
    ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        snapshot.timestamp = std::chrono::system_clock::now();
        @autoreleasepool {
            NSRunningApplication* app = [[NSWorkspace sharedWorkspace] frontmostApplication];
            pid_t pid = 0;
            if (app) {
                pid = [app processIdentifier];
                snapshot.processName = utf8([app localizedName]);
                if (NSURL* url = [app executableURL]) {
                    snapshot.exePath = utf8([url path]);
                    // Keep the application name on macOS. Many desktop apps share a generic
                    // executable such as "Electron", which is not a useful activity identity.
                    if (snapshot.processName.empty()) snapshot.processName = utf8([[url path] lastPathComponent]);
                }
            }

            const CFTimeInterval idle = CGEventSourceSecondsSinceLastEventType(
                kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
            snapshot.idleSeconds = static_cast<int>(idle);

            if (pid != 0) {
                const bool browser = isBrowserProcess(snapshot.processName);
                const auto accessibility = accessibilityInfo(pid, browser);
                snapshot.windowTitle = accessibility.title;
                if (accessibility.browserAddress) {
                    snapshot.browserUrl = accessibility.browserAddress->url;
                    snapshot.browserDomain = accessibility.browserAddress->domain;
                }
            }

            // Accessibility can be denied. Retain the old CGWindow title fallback so tracking still
            // has useful window names when Screen Recording permission is available instead.
            if (snapshot.windowTitle.empty()) {
                CFArrayRef list = CGWindowListCopyWindowInfo(
                    kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                    kCGNullWindowID);
                if (list) {
                    for (NSDictionary* entry in (__bridge NSArray*)list) {
                        NSNumber* layer = entry[(id)kCGWindowLayer];
                        if ([layer intValue] != 0) continue;
                        NSString* owner = entry[(id)kCGWindowOwnerName];
                        if (app && [owner isEqualToString:[app localizedName]]) {
                            snapshot.windowTitle = utf8(entry[(id)kCGWindowName]);
                            break;
                        }
                    }
                    CFRelease(list);
                }
            }
            if (isBrowserProcess(snapshot.processName) && snapshot.browserUrl.empty()) {
                if (const auto address = findBrowserAddressInText(snapshot.windowTitle)) {
                    snapshot.browserUrl = address->url;
                    snapshot.browserDomain = address->domain;
                }
            }
        }
        return snapshot;
    }

    ProviderCapabilities capabilities() const override {
        return {true, true, true, true,
                "macOS backend. Window title/browser URL may require Accessibility or Screen Recording permission."};
    }
};

std::unique_ptr<IActivityProvider> createActivityProvider() {
    return std::make_unique<MacActivityProvider>();
}
} // namespace pcat
