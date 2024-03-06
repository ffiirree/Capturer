#ifndef CAPTURER_QUEUE_H
#define CAPTURER_QUEUE_H

#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template<class T> class safe_queue
{
public:
    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;

    safe_queue() = default;

    safe_queue(const safe_queue&)            = delete;
    safe_queue& operator=(const safe_queue&) = delete;

    explicit safe_queue(const size_t capacity)
        : capacity_(capacity)
    {}

    // capacity

    [[nodiscard]] bool empty() const noexcept(noexcept(buffer_.empty()))
    {
        std::lock_guard lock(mtx_);
        return buffer_.empty();
    }

    [[nodiscard]] auto size() const noexcept(noexcept(buffer_.size()))
    {
        std::lock_guard lock(mtx_);
        return buffer_.size();
    }

    [[nodiscard]] size_t capacity() const noexcept
    {
        std::lock_guard lock(mtx_);
        return capacity_;
    }

    [[nodiscard]] bool stopped() const noexcept
    {
        std::lock_guard lock(mtx_);
        return stopped_;
    }

    // modifiers

    [[nodiscard]] std::optional<value_type> wait_and_pop()
    {
        std::unique_lock lock(mtx_);
        nonempty_.wait(lock, [this] { return stopped_ || !buffer_.empty(); });

        if (buffer_.empty()) return std::nullopt;

        value_type front = std::move(buffer_.front());
        buffer_.pop();

        nonfull_.notify_one();

        return front;
    }

    [[nodiscard]] std::optional<value_type> pop()
    {
        std::lock_guard lock(mtx_);

        if (buffer_.empty()) return std::nullopt;

        value_type front = std::move(buffer_.front());
        buffer_.pop();

        nonfull_.notify_one();

        return front;
    }

    bool wait_and_push(const value_type& value)
    {
        std::unique_lock lock(mtx_);
        nonfull_.wait(lock, [this] { return stopped_ || buffer_.size() < capacity_; });

        if (stopped_) return false;

        buffer_.push(value);

        nonempty_.notify_one();

        return true;
    }

    template<class Rep, class Period>
    bool wait_and_push(const value_type& value, const std::chrono::duration<Rep, Period>& duration)
    {
        std::unique_lock lock(mtx_);
        if (!nonfull_.wait_for(lock, duration, [this] { return stopped_ || buffer_.size() < capacity_; })) {
            return false;
        }

        if (stopped_) return false;

        buffer_.push(value);

        nonempty_.notify_one();

        return true;
    }

    bool push(const value_type& value, const bool discard = false)
    {
        std::lock_guard lock(mtx_);

        if (stopped_) return false;

        if (buffer_.size() >= capacity_) {
            if (!discard) {
                return false;
            }
            buffer_.pop();
        }

        buffer_.push(value);

        nonempty_.notify_one();

        return true;
    }

    bool wait_and_push(value_type&& value)
    {
        std::unique_lock lock(mtx_);
        nonfull_.wait(lock, [this] { return stopped_ || buffer_.size() < capacity_; });

        if (stopped_) return false;

        buffer_.push(std::move(value));

        nonempty_.notify_one();

        return true;
    }

    template<class Rep, class Period>
    bool wait_and_push(value_type&& value, const std::chrono::duration<Rep, Period>& duration)
    {
        std::unique_lock lock(mtx_);
        if (!nonfull_.wait_for(lock, duration, [this] { return stopped_ || buffer_.size() < capacity_; })) {
            return false;
        }

        if (stopped_) return false;

        buffer_.push(std::move(value));

        nonempty_.notify_one();

        return true;
    }

    bool push(value_type&& value, const bool discard = false)
    {
        std::lock_guard lock(mtx_);

        if (stopped_) return false;

        if (buffer_.size() >= capacity_) {
            if (!discard) {
                return false;
            }
            buffer_.pop();
        }

        buffer_.push(std::move(value));

        nonempty_.notify_one();

        return true;
    }

    void drain()
    {
        std::lock_guard lock(mtx_);

        buffer_ = {};

        nonfull_.notify_all();
        nonempty_.notify_all();
    }

    void start()
    {
        std::lock_guard lock(mtx_);

        stopped_ = false;

        nonempty_.notify_all();
        nonfull_.notify_all();
    }

    void stop()
    {
        std::lock_guard lock(mtx_);

        stopped_ = true;
        buffer_  = {};

        nonempty_.notify_all();
        nonfull_.notify_all();
    }

    void notify_all()
    {
        std::lock_guard lock(mtx_);

        nonempty_.notify_all();
        nonfull_.notify_all();
    }

private:
    mutable std::mutex      mtx_;
    std::condition_variable nonempty_{};
    std::condition_variable nonfull_{};

    size_t capacity_{ std::numeric_limits<size_t>::max() };

    bool stopped_{};

    std::queue<T> buffer_{};
};

#endif //! CAPTURER_QUEUE_H