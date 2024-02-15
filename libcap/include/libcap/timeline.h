#ifndef CAPTURER_TIMELINE_H
#define CAPTURER_TIMELINE_H

#include "clock.h"
#include "rational.h"

#include <shared_mutex>

namespace av
{
    struct timeline_t
    {
        using duration = std::chrono::nanoseconds;

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

        [[nodiscard]] duration time() const
        {
            std::shared_lock lock(mtx_);

            return _time();
        }

        template<class Rep, class Period>
        void set(const std::chrono::duration<Rep, Period>& ts,
                 const std::chrono::duration<Rep, Period>& base = clock::nopts)
        {
            std::unique_lock lock(mtx_);

            escaped_ = ts;
            updated_ = (base == clock::nopts) ? clock::us() : base;
        }

        void pause()
        {
            std::unique_lock lock(mtx_);

            escaped_ = _time();
            updated_ = clock::us();
            paused_  = true;
        }

        bool paused() const
        {
            std::shared_lock lock(mtx_);

            return paused_;
        }

        void resume()
        {
            std::unique_lock lock(mtx_);

            updated_ = clock::us();
            paused_  = false;
        }

        [[nodiscard]] rational speed() const
        {
            std::shared_lock lock(mtx_);

            return speed_;
        }

        void set_speed(const rational value)
        {
            std::unique_lock lock(mtx_);

            escaped_ = _time();
            updated_ = clock::us();
            speed_   = value;
        }

        bool invalid() const
        {
            std::shared_lock lock(mtx_);

            return (escaped_ == clock::nopts) || (updated_ == clock::nopts);
        }

    private:
        // | ---- escaped ----- | ------- > now
        //                      ^
        //                      |
        //                    updated
        [[nodiscard]] duration _time() const
        {
            return escaped_ + (paused_ ? duration{ 0 } : (clock::ns() - updated_) * speed_);
        }

        mutable std::shared_mutex mtx_{};

        duration escaped_{ clock::nopts };

        duration updated_{ clock::nopts };

        rational speed_{ 1, 1 };

        bool paused_{ false };
    };
} // namespace av

#endif //! CAPTURER_TIMELINE_H