#ifndef CAPTURER_SUBTITLE_H
#define CAPTURER_SUBTITLE_H

#include "libcap/clock.h"
#include "libcap/ffmpeg-wrapper.h"
#include "libcap/rational.h"

#include <ass/ass.h>
#include <memory>
#include <mutex>

struct Subtitle
{
    av::rational<int> x{};
    av::rational<int> y{};
    av::rational<int> w{};
    av::rational<int> h{};

    int stride{};

    std::chrono::nanoseconds pts{ av::clock::nopts };
    std::chrono::nanoseconds duration{ av::clock::nopts };

    AVPixelFormat format{ AV_PIX_FMT_PAL8 };

    uint8_t color[4]{}; // RGBA

    size_t                     size{};
    std::shared_ptr<uint8_t[]> buffer{};
};

#endif //! CAPTURER_SUBTITLE_H