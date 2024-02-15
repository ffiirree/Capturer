#ifndef CAPTURER_CLOCK_H
#define CAPTURER_CLOCK_H

#include <chrono>

extern "C" {
#include <libavutil/avutil.h>
}

// clang-format off
#define OS_TIME_BASE        1000000000 // ns
#define OS_TIME_BASE_Q      { 1, OS_TIME_BASE }
// clang-format on

using namespace std::chrono_literals;

namespace av::clock
{
    constexpr auto nopts = std::chrono::nanoseconds::min(); // == AV_NOPTS_VALUE

    constexpr auto max = std::chrono::nanoseconds::max();
    constexpr auto min = std::chrono::nanoseconds::min();

    /**
     * Get the current time in nanoseconds since some unspecified starting point.
     * On platforms that support it, the time comes from a monotonic clock
     * This property makes this time source ideal for measuring relative time.
     * The returned values may not be monotonic on platforms where a monotonic
     * clock is not available.
     */
    inline auto ns()
    {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch());
    }

    inline auto us()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch());
    }

    inline auto ms()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    }

    inline auto ns(const int64_t pts, const AVRational timebase)
    {
        return std::chrono::nanoseconds{ av_rescale_q(pts, timebase, { 1, 1000000000 }) };
    }
} // namespace av::clock

#endif //! CAPTURER_CLOCK_H