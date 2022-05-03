#ifndef CAPTURER_RING_BUFFER_H
#define CAPTURER_RING_BUFFER_H

#include <mutex>
#include "logging.h"

template<class T, int N>
class RingBuffer {
public:
	void push(const T& v)
	{
		LOG(INFO) << "pushed";
		std::lock_guard<std::mutex> lock(mtx_);

		if ((pushed_idx_ + 1) % N == popped_idx_) {
			filled_ = true;
		}

		// covered
		if (filled_ && (pushed_idx_ == popped_idx_)) {
			popped_idx_ = (popped_idx_ + 1) % N;	
		}

		// push
		buffer_[pushed_idx_] = v;
		pushed_idx_ = (pushed_idx_ + 1) % N;
	}

	T pop()
	{
		std::lock_guard<std::mutex> lock(mtx_);

		auto ret = (!filled_ && (pushed_idx_ == popped_idx_)) ? buffer_[(popped_idx_ + N - 1) % N] : buffer_[popped_idx_];
		if (filled_ || (pushed_idx_ != popped_idx_)) {
			popped_idx_ = (popped_idx_ + 1) % N;
		}

		filled_ = false;
		return ret;
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(vmutx_);
		popped_idx_ = 0;
		pushed_idx_ = 0;
	}

	bool empty() 
	{ 
		std::lock_guard<std::mutex> lock(mtx_); 
		return !filled_ && (pushed_idx_ == popped_idx_);
	}
	
	size_t size() 
	{ 
		std::lock_guard<std::mutex> lock(mtx_);

		if (filled_) {
			return N;
		}
		return ((pushed_idx_ >= popped_idx_) ? (pushed_idx_) : (pushed_idx_ + N)) - popped_idx_;
	}
	
private:
	size_t pushed_idx_{ 0 };
	size_t popped_idx_{ 0 };
	bool filled_{ false };

	T buffer_[N];
	std::mutex mtx_;
};

#endif // !CAPTURER_RING_BUFFER_H