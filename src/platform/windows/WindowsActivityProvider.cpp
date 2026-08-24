#include "platform/Factory.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <uiautomation.h>

#include "core/BrowserAddress.h"
#include "core/ExclusionMatcher.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pcat {
namespace {

template <class T>
void releaseCom(T*& value) noexcept {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                            result.data(), size, nullptr, nullptr) <= 0) {
        return {};
    }
    return result;
}

class UiAutomationContext {
public:
    UiAutomationContext() {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        uninitialize_ = init == S_OK || init == S_FALSE;
        if (FAILED(init) && init != RPC_E_CHANGED_MODE) return;
        if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_IUIAutomation, reinterpret_cast<void**>(&automation_)))) {
            automation_ = nullptr;
        }
    }
    ~UiAutomationContext() {
        releaseCom(automation_);
        if (uninitialize_) CoUninitialize();
    }
    UiAutomationContext(const UiAutomationContext&) = delete;
    UiAutomationContext& operator=(const UiAutomationContext&) = delete;

    std::optional<BrowserAddress> addressBar(HWND window) const {
        if (!automation_ || !window) return std::nullopt;

        IUIAutomationElement* root = nullptr;
        if (FAILED(automation_->ElementFromHandle(window, &root)) || !root) return std::nullopt;

        VARIANT value{};
        VariantInit(&value);
        value.vt = VT_I4;
        value.lVal = UIA_EditControlTypeId;
        IUIAutomationCondition* condition = nullptr;
        HRESULT hr = automation_->CreatePropertyCondition(UIA_ControlTypePropertyId, value, &condition);
        VariantClear(&value);
        if (FAILED(hr) || !condition) {
            releaseCom(root);
            return std::nullopt;
        }

        IUIAutomationElementArray* edits = nullptr;
        hr = root->FindAll(TreeScope_Descendants, condition, &edits);
        releaseCom(condition);
        releaseCom(root);
        if (FAILED(hr) || !edits) return std::nullopt;

        int length = 0;
        edits->get_Length(&length);
        std::optional<BrowserAddress> result;
        for (int index = 0; index < length && !result; ++index) {
            IUIAutomationElement* element = nullptr;
            if (FAILED(edits->GetElement(index, &element)) || !element) continue;

            IUIAutomationValuePattern* pattern = nullptr;
            if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId, IID_IUIAutomationValuePattern,
                                                       reinterpret_cast<void**>(&pattern))) && pattern) {
                BSTR current = nullptr;
                if (SUCCEEDED(pattern->get_CurrentValue(&current)) && current) {
                    result = parseBrowserAddress(utf8(std::wstring(current, SysStringLen(current))));
                    SysFreeString(current);
                }
                releaseCom(pattern);
            }

            if (!result) {
                BSTR name = nullptr;
                if (SUCCEEDED(element->get_CurrentName(&name)) && name) {
                    result = parseBrowserAddress(utf8(std::wstring(name, SysStringLen(name))));
                    SysFreeString(name);
                }
            }
            releaseCom(element);
        }
        releaseCom(edits);
        return result;
    }

private:
    IUIAutomation* automation_{};
    bool uninitialize_{};
};

UiAutomationContext& uiAutomation() {
    thread_local UiAutomationContext context;
    return context;
}

} // namespace

class WindowsActivityProvider final : public IActivityProvider {
public:
    ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        snapshot.timestamp = std::chrono::system_clock::now();
        HWND window = GetForegroundWindow();
        if (!window) return snapshot;

        const int titleLength = GetWindowTextLengthW(window);
        std::wstring title;
        if (titleLength > 0) {
            title.resize(static_cast<std::size_t>(titleLength) + 1);
            const int copied = GetWindowTextW(window, title.data(), titleLength + 1);
            title.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0);
        }

        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        std::wstring path;
        if (process) {
            DWORD size = 32768;
            path.resize(size);
            if (QueryFullProcessImageNameW(process, 0, path.data(), &size)) path.resize(size);
            else path.clear();
            CloseHandle(process);
        }

        snapshot.windowTitle = utf8(title);
        snapshot.exePath = utf8(path);
        if (!path.empty()) snapshot.processName = utf8(std::filesystem::path(path).filename().wstring());

        LASTINPUTINFO input{sizeof(input)};
        if (GetLastInputInfo(&input)) {
            // LASTINPUTINFO::dwTime and GetTickCount() are both 32-bit values and unsigned subtraction
            // intentionally handles the ~49.7 day wrap-around.
            const DWORD elapsed = GetTickCount() - input.dwTime;
            snapshot.idleSeconds = static_cast<int>(elapsed / 1000U);
        }

        if (isBrowserProcess(snapshot.processName)) {
            auto address = uiAutomation().addressBar(window);
            if (!address) address = findBrowserAddressInText(snapshot.windowTitle);
            if (address) {
                snapshot.browserUrl = std::move(address->url);
                snapshot.browserDomain = std::move(address->domain);
            }
        }
        return snapshot;
    }

    ProviderCapabilities capabilities() const override {
        return {true, true, true, true,
                "Windows backend: active window, idle time and browser URL via UI Automation."};
    }
};

std::unique_ptr<IActivityProvider> createActivityProvider() {
    return std::make_unique<WindowsActivityProvider>();
}
} // namespace pcat
#endif
