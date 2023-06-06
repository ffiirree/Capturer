#ifndef CAPTURER_RING_VECTOR_H
#define CAPTURER_RING_VECTOR_H

#include <functional>
#include <mutex>

#define EMPTY (!full_ && (pushed_idx_ == popped_idx_))

template<class T, int N> class RingVector
{
public:
    explicit RingVector(
        std::function<T()> allocate         = []() { return T{}; },
        std::function<void(T *)> deallocate = [](T *) {})
    {
        allocate_   = allocate;
        deallocate_ = deallocate;

        for (size_t i = 0; i < N; i++) {
            buffer_[i] = allocate_();
        }
    }

    ~RingVector()
    {
        for (size_t i = 0; i < N; i++) {
            deallocate_(&buffer_[i]);
        }
    }

    void push(std::function<void(T)> callback)
    {
        std::lock_guard<std::mutex> lock(mtx_);

        // last one
        //
        //                   PUSH | POP
        // ------------------------------------------------
        // |  -  |  -  | ... |    |  -  | ... |  -  |  -  |
        // ------------------------------------------------
        if ((pushed_idx_ + 1) % N == popped_idx_) {
            full_ = true;
        }

        // full & covered
        if (full_ && (pushed_idx_ == popped_idx_)) {
            popped_idx_ = (popped_idx_ + 1) % N;
        }

        // push
        callback(buffer_[pushed_idx_]);

        pushed_idx_ = (pushed_idx_ + 1) % N;
    }

    void pop(std::function<void(T)> callback = [](T) {})
    {
        std::lock_guard<std::mutex> lock(mtx_);

        // empty ? last : next
        callback(EMPTY ? buffer_[(popped_idx_ + N - 1) % N] : buffer_[popped_idx_]);

        // !empty
        if (!EMPTY) {
            popped_idx_ = (popped_idx_ + 1) % N;
        }

        full_ = false;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        popped_idx_ = 0;
        pushed_idx_ = 0;
        full_       = false;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return EMPTY;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);

        if (full_) {
            return N;
        }
        return ((pushed_idx_ >= popped_idx_) ? (pushed_idx_) : (pushed_idx_ + N)) - popped_idx_;
    }

    bool full() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return full_;
    }

private:
    std::function<T()> allocate_{ []() { return T{}; } };
    std::function<void(T *)> deallocate_{ [](T *) {} };
    size_t pushed_idx_{ 0 };
    size_t popped_idx_{ 0 };
    bool full_{ false };

    T buffer_[N]{};
    mutable std::mutex mtx_;
};
#undef EMPTY
#endif //! CAPTURER_RING_VECTOR_H