#include "ui/TrayIcon.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
// libstdc++ на MinGW уже задаёт NOMINMAX=1, поэтому определять его безусловно нельзя.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>
#include <vector>

#ifndef PCAT_ICON_RESOURCE_ID
#define PCAT_ICON_RESOURCE_ID 101
#endif

namespace pcat {
namespace {

struct IconInfoGuard {
    ICONINFO info{};
    ~IconInfoGuard() {
        if (info.hbmColor) DeleteObject(info.hbmColor);
        if (info.hbmMask) DeleteObject(info.hbmMask);
    }
};

struct ScreenDcGuard {
    HDC dc{GetDC(nullptr)};
    ~ScreenDcGuard() { if (dc) ReleaseDC(nullptr, dc); }
};

BITMAPINFO topDownBgra(int width, int height) {
    BITMAPINFO desc{};
    desc.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    desc.bmiHeader.biWidth = width;
    desc.bmiHeader.biHeight = -height; // отрицательная высота — строки сверху вниз, как в SDL
    desc.bmiHeader.biPlanes = 1;
    desc.bmiHeader.biBitCount = 32;
    desc.bmiHeader.biCompression = BI_RGB;
    return desc;
}

SDL_Surface* surfaceFromIcon(HICON icon) {
    if (!icon) return nullptr;

    IconInfoGuard guard;
    if (!GetIconInfo(icon, &guard.info) || !guard.info.hbmColor) return nullptr;

    BITMAP header{};
    if (GetObjectW(guard.info.hbmColor, sizeof(header), &header) == 0) return nullptr;
    const int width = header.bmWidth;
    const int height = header.bmHeight;
    if (width <= 0 || height <= 0) return nullptr;

    ScreenDcGuard screen;
    if (!screen.dc) return nullptr;

    auto desc = topDownBgra(width, height);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    if (GetDIBits(screen.dc, guard.info.hbmColor, 0, static_cast<UINT>(height),
                  pixels.data(), &desc, DIB_RGB_COLORS) == 0) {
        return nullptr;
    }

    // Иконки без собственного альфа-канала прозрачны только через маску: там 1 — прозрачный пиксель.
    bool hasAlpha = false;
    for (std::size_t index = 3; index < pixels.size(); index += 4) {
        if (pixels[index] != 0) { hasAlpha = true; break; }
    }
    if (!hasAlpha) {
        std::vector<unsigned char> mask(pixels.size());
        auto maskDesc = topDownBgra(width, height);
        if (guard.info.hbmMask && GetDIBits(screen.dc, guard.info.hbmMask, 0, static_cast<UINT>(height),
                                            mask.data(), &maskDesc, DIB_RGB_COLORS) != 0) {
            for (std::size_t index = 0; index < pixels.size(); index += 4) {
                pixels[index + 3] = mask[index] ? 0U : 255U;
            }
        } else {
            for (std::size_t index = 3; index < pixels.size(); index += 4) pixels[index] = 255U;
        }
    }

    // ARGB8888 в SDL на little-endian лежит в памяти как B,G,R,A — ровно то, что отдаёт GetDIBits.
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return nullptr;
    if (!SDL_LockSurface(surface)) { SDL_DestroySurface(surface); return nullptr; }
    auto* destination = static_cast<unsigned char*>(surface->pixels);
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4U;
    for (int row = 0; row < height; ++row) {
        std::memcpy(destination + static_cast<std::size_t>(row) * static_cast<std::size_t>(surface->pitch),
                    pixels.data() + static_cast<std::size_t>(row) * rowBytes,
                    rowBytes);
    }
    SDL_UnlockSurface(surface);
    return surface;
}

} // namespace

SDL_Surface* loadApplicationIconSurface(IconSize size) {
    const int requested = size == IconSize::Tray ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON);
    HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
                                               MAKEINTRESOURCEW(PCAT_ICON_RESOURCE_ID),
                                               IMAGE_ICON,
                                               requested > 0 ? requested : 0,
                                               requested > 0 ? requested : 0,
                                               LR_DEFAULTCOLOR));
    if (!icon) return nullptr;
    SDL_Surface* surface = surfaceFromIcon(icon);
    DestroyIcon(icon); // LR_SHARED не использован, значит иконку освобождаем сами.
    return surface;
}
} // namespace pcat

#else

namespace pcat {
// На остальных платформах иконка в исполняемый файл не встраивается: вызывающая сторона
// использует запасной вариант.
SDL_Surface* loadApplicationIconSurface(IconSize) { return nullptr; }
}

#endif
