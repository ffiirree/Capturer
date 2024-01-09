#ifndef CAPTURER_CLOCK_H
#define CAPTURER_CLOCK_H

#include <chrono>
#include <mutex>

extern "C" {
#include <libavutil/avutil.h>
}

#define OS_TIME_BASE 1'000'000'000
#define OS_TIME_BASE_Q                                                                                     \
    {                                                                                                      \
        1, OS_TIME_BASE                                                                                    \
    }

using namespace std::chrono_literals;

namespace av
{
    namespace clock
    {
        constexpr auto nopts = std::chrono::nanoseconds::max();

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
            return std::chrono::nanoseconds{ av_rescale_q(pts, timebase, { 1, 1'000'000'000 }) };
        }
    } // namespace clock

    struct rational
    {
        intmax_t num{};
        intmax_t den{};

        template<typename T> T get() { return static_cast<T>(num) / static_cast<T>(den); }
    };

    template<class Rep, class Period>
    std::chrono::duration<Rep, Period> operator/(const std::chrono::duration<Rep, Period>& ts,
                                                 const rational speed)
    {

        return ts * speed.den / speed.num;
    }

    template<class Rep, class Period>
    std::chrono::duration<Rep, Period> operator*(const std::chrono::duration<Rep, Period>& ts,
                                                 const rational speed)
    {
        return ts * speed.num / speed.den;
    }

    struct timeline_t
    {
        timeline_t() = default;

        template<class Rep, class Period> explicit timeline_t(const std::chrono::duration<Rep, Period>& ts)
        {
            set(ts);
        }

        template<class Rep, class Period>
        timeline_t& operator=(const std::chrono::duration<Rep, Period>& ts)
        {
            set(ts);

            return *this;
        }

        using duration = std::chrono::nanoseconds;

        [[nodiscard]] duration time()
        {
            std::lock_guard lock(mtx_);

            return _time();
        }

        template<class Rep, class Period>
        void set(const std::chrono::duration<Rep, Period>& ts,
                 const std::chrono::duration<Rep, Period>& base = clock::nopts)
        {
            std::lock_guard lock(mtx_);

            ts_      = ts;
            updated_ = base == clock::nopts ? clock::us() : base;
        }

        void pause()
        {
            std::lock_guard lock(mtx_);

            ts_      = _time();
            updated_ = clock::us();

            paused_ = true;
        }

        void resume()
        {
            std::lock_guard lock(mtx_);

            updated_ = clock::us();

            paused_ = false;
        }

        [[nodiscard]] rational speed()
        {
            std::lock_guard lock(mtx_);

            return speed_;
        }

        void set_speed(const rational value)
        {
            std::lock_guard lock(mtx_);

            ts_      = _time();
            updated_ = clock::us();

            speed_ = value;
        }

        bool invalid()
        {
            std::lock_guard lock(mtx_);

            return ts_ == clock::nopts || updated_ == clock::nopts;
        }

        bool paused()
        {
            std::lock_guard lock(mtx_);

            return paused_;
        }

    private:
        [[nodiscard]] duration _time() const
        {
            if (paused_) return ts_;

            return ts_ + (clock::ns() - updated_) * speed_.num / speed_.den;
        }

        std::mutex mtx_{};

        duration ts_{ clock::nopts };

        duration updated_{ clock::nopts };

        rational speed_{ 1, 1 };
        bool paused_{ false };
    };
} // namespace av

#endif //! CAPTURER_CLOCK_H