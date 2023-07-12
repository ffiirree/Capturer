#ifndef CAPTURER_CLOCK_H
#define CAPTURER_CLOCK_H

#include <chrono>
#include <thread>

#define OS_TIME_BASE            1'000'000'000
#define OS_TIME_BASE_Q          { 1, OS_TIME_BASE }

using namespace std::chrono_literals;

/**
 * Get the current time in nanoseconds since some unspecified starting point.
 * On platforms that support it, the time comes from a monotonic clock
 * This property makes this time source ideal for measuring relative time.
 * The returned values may not be monotonic on platforms where a monotonic
 * clock is not available.
 */
inline int64_t os_gettime_ns()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

inline int64_t os_gettime_us()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

inline int64_t os_gettime_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// sleep
template <class Rep, class Period>
void os_sleep(const std::chrono::duration<Rep, Period>& _t) 
{
    std::this_thread::sleep_for(_t);
}

inline void os_nsleep(int64_t _ns) 
{ 
    std::this_thread::sleep_for(std::chrono::nanoseconds(_ns)); 
}

inline void os_usleep(int64_t _us)
{
    std::this_thread::sleep_for(std::chrono::microseconds(_us));
}

inline void os_msleep(int64_t _ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(_ms));
}

#endif //! CAPTURER_CLOCK_H