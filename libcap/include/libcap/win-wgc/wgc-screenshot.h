#ifndef CAPTURER_WGC_SCREENSHOT_H
#define CAPTURER_WGC_SCREENSHOT_H

#ifdef _WIN32

#include "libcap/ffmpeg-wrapper.h"

#include <d3d11.h>
#include <Windows.h>
#include <winrt/base.h>

namespace wgc
{
    // window
    winrt::com_ptr<ID3D11Texture2D> screenshot(HWND hwnd);

    // monitor
    winrt::com_ptr<ID3D11Texture2D> screenshot(HMONITOR hmon);
} // namespace wgc

#endif

#endif //! CAPTURER_WGC_SCREENSHOT_H