#ifndef CAPTURER_RATIONAL_H
#define CAPTURER_RATIONAL_H

#include <chrono>

namespace av
{
    struct rational
    {
        intmax_t num{ 0 };
        intmax_t den{ 1 };

        template<typename T> T get() { return static_cast<T>(num) / static_cast<T>(den); }
    };

    template<class Rep, class Period>
    std::chrono::duration<Rep, Period> operator/(const std::chrono::duration<Rep, Period>& ts,
                                                 const rational                            speed)
    {

        return ts * speed.den / speed.num;
    }

    template<class Rep, class Period>
    std::chrono::duration<Rep, Period> operator*(const std::chrono::duration<Rep, Period>& ts,
                                                 const rational                            speed)
    {
        return ts * speed.num / speed.den;
    }
} // namespace av

#endif //! CAPTURER_RATIONAL_H