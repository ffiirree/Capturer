#ifndef CAPTURER_RING_BUFFER_H
#define CAPTURER_RING_BUFFER_H

#include <mutex>

template<class T, int N>
class RingBuffer {
public:
	void push(std::function<void(T)> callback)
	{
		std::lock_guard<std::mutex> lock(mtx_);

		if ((pushed_idx_ + 1) % N == popped_idx_) {
			filled_ = true;
		}

		// covered
		if (filled_ && (pushed_idx_ == popped_idx_)) {
			popped_idx_ = (popped_idx_ + 1) % N;	
		}

		// push
		callback(buffer_[pushed_idx_]);
		pushed_idx_ = (pushed_idx_ + 1) % N;
		unused_ = false;
	}

	void pop(std::function<void(T)> callback = [](T) {})
	{
		std::lock_guard<std::mutex> lock(mtx_);

		auto popped = (!filled_ && (pushed_idx_ == popped_idx_)) ? buffer_[(popped_idx_ + N - 1) % N] : buffer_[popped_idx_];
		callback(popped);

		if (filled_ || (pushed_idx_ != popped_idx_)) {
			popped_idx_ = (popped_idx_ + 1) % N;
		}

		filled_ = false;
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		popped_idx_ = 0;
		pushed_idx_ = 0;
		unused_ = true;
	}

	bool empty() 
	{ 
		std::lock_guard<std::mutex> lock(mtx_); 
		return !filled_ && (pushed_idx_ == popped_idx_);
	}

	bool unused()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return unused_;
	}
	
	size_t size() 
	{ 
		std::lock_guard<std::mutex> lock(mtx_);

		if (filled_) {
			return N;
		}
		return ((pushed_idx_ >= popped_idx_) ? (pushed_idx_) : (pushed_idx_ + N)) - popped_idx_;
	}

	T& operator[](size_t idx) { return buffer_[std::min<size_t>(std::max<size_t>(idx, 0), N - 1)]; }
	
private:
	size_t pushed_idx_{ 0 };
	size_t popped_idx_{ 0 };
	bool filled_{ false };
	bool unused_{ true };

	T buffer_[N];
	std::mutex mtx_;
};

#endif // !CAPTURER_RING_BUFFER_H