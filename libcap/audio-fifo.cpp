#include "libcap/audio-fifo.h"

safe_audio_fifo::safe_audio_fifo(AVSampleFormat sample_fmt, int channels, int size)
{
    buffer_ = av_audio_fifo_alloc(sample_fmt, channels, size);
}

safe_audio_fifo::~safe_audio_fifo()
{
    std::lock_guard lock(mtx_);
    av_audio_fifo_free(buffer_);
}

bool safe_audio_fifo::empty() const
{
    std::lock_guard lock(mtx_);
    return av_audio_fifo_size(buffer_) == 0;
}

int safe_audio_fifo::size() const
{
    std::lock_guard lock(mtx_);
    return av_audio_fifo_size(buffer_);
}

int safe_audio_fifo::capacity() const
{
    std::lock_guard lock(mtx_);
    return av_audio_fifo_size(buffer_) + av_audio_fifo_space(buffer_);
}

int safe_audio_fifo::reserve(int nb_samples)
{
    std::lock_guard lock(mtx_);
    return av_audio_fifo_realloc(buffer_, nb_samples);
}

int safe_audio_fifo::write(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);
    return av_audio_fifo_write(buffer_, data, nb_samples);
}

int safe_audio_fifo::read(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);
    return av_audio_fifo_read(buffer_, data, nb_samples);
}

int safe_audio_fifo::peek(void **data, int nb_samples)
{
    std::unique_lock lock(mtx_);
    return av_audio_fifo_peek(buffer_, data, nb_samples);
}

void safe_audio_fifo::clear()
{
    std::unique_lock lock(mtx_);
    av_audio_fifo_reset(buffer_);
}
