#include "libcap/audio-fifo.h"

safe_audio_fifo::safe_audio_fifo(AVSampleFormat sample_fmt, int channels, int capacity)
    : capacity_(capacity)
{
    buffer_ = av_audio_fifo_alloc(sample_fmt, channels, capacity);
}

safe_audio_fifo::~safe_audio_fifo()
{
    std::lock_guard lock(mtx_);
    av_audio_fifo_free(buffer_);
}

bool safe_audio_fifo::empty() const
{
    std::lock_guard lock(mtx_);
    return stopped_ || av_audio_fifo_size(buffer_) == 0;
}

int safe_audio_fifo::size() const
{
    std::lock_guard lock(mtx_);
    return stopped_ ? 0 : av_audio_fifo_size(buffer_);
}

int safe_audio_fifo::capacity() const
{
    std::lock_guard lock(mtx_);
    return capacity_;
}

int safe_audio_fifo::reserve(int capacity)
{
    std::lock_guard lock(mtx_);

    capacity_ = capacity;

    const int ret = av_audio_fifo_realloc(buffer_, capacity_);

    nonfull_.notify_all();

    return ret;
}

int safe_audio_fifo::write(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);

    if (nb_samples > capacity_ - av_audio_fifo_size(buffer_)) return 0;

    nb_samples = av_audio_fifo_write(buffer_, data, nb_samples);

    nonempty_.notify_all();

    return nb_samples;
}

int safe_audio_fifo::wait_and_write(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);
    nonfull_.wait(lock,
                  [=, this] { return stopped_ || nb_samples < capacity_ - av_audio_fifo_size(buffer_); });

    if (stopped_) return 0;

    nb_samples = av_audio_fifo_write(buffer_, data, nb_samples);

    nonempty_.notify_all();

    return nb_samples;
}

int safe_audio_fifo::read(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);

    nb_samples = av_audio_fifo_read(buffer_, data, nb_samples);

    nonfull_.notify_all();

    return nb_samples;
}

int safe_audio_fifo::wait_and_read(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);
    nonempty_.wait(lock, [=, this] { return stopped_ || nb_samples <= av_audio_fifo_size(buffer_); });

    nb_samples = av_audio_fifo_read(buffer_, data, nb_samples);

    nonfull_.notify_all();

    return nb_samples;
}

int safe_audio_fifo::peek(void **data, int nb_samples) const
{
    std::unique_lock lock(mtx_);
    return av_audio_fifo_peek(buffer_, data, nb_samples);
}

void safe_audio_fifo::drain()
{
    std::unique_lock lock(mtx_);

    av_audio_fifo_reset(buffer_);

    nonfull_.notify_all();
}

void safe_audio_fifo::stop()
{
    std::unique_lock lock(mtx_);

    stopped_ = true;

    nonfull_.notify_all();
    nonempty_.notify_all();
}

void safe_audio_fifo::start()
{
    std::unique_lock lock(mtx_);

    stopped_ = false;

    nonfull_.notify_all();
    nonempty_.notify_all();
}

