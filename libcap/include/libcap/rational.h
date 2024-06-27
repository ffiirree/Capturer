#ifndef CAPTURER_RATIONAL_H
#define CAPTURER_RATIONAL_H

#include <chrono>

namespace av
{
    template<typename I>
    requires std::integral<I>
    struct rational
    {
        I num{ 0 };
        I den{ 1 };

        template<typename R> R get() { return static_cast<R>(num) / static_cast<R>(den); }
    };

    template<class Rep, class Period>
    std::chrono::duration<Rep, Period> operator/(const std::chrono::duration<Rep, Period>& ts,
                                                 const rational<intmax_t>                  speed)
    {

        return ts * speed.den / speed.num;
    }

    template<class Rep, class Period>
    std::chrono::duration<Rep, Period> operator*(const std::chrono::duration<Rep, Period>& ts,
                                                 const rational<intmax_t>                  speed)
    {
        return ts * speed.num / speed.den;
    }
} // namespace av

#endif //! CAPTURER_RATIONAL_H