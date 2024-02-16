#ifndef CAPTURER_AuDIO_FIFO_H
#define CAPTURER_AuDIO_FIFO_H

#include <mutex>

extern "C" {
#include <libavutil/audio_fifo.h>
}

class safe_audio_fifo
{
public:
    explicit safe_audio_fifo(AVSampleFormat sample_fmt, int channels, int size);

    safe_audio_fifo(const safe_audio_fifo&)            = delete;
    safe_audio_fifo& operator=(const safe_audio_fifo&) = delete;

    ~safe_audio_fifo();

    bool empty() const;

    int size() const;

    int capacity() const;

    int reserve(int nb_samples);

    /**
     * Write data to the AVAudioFifo.
     *
     * The safe_audio_fifo will be reallocated automatically if the available space
     * is less than nb_samples.
     *
     * @param data        audio data plane pointers
     * @param nb_samples  number of samples to write
     * @return            number of samples actually written, or negative AVERROR
     *                    code on failure. If successful, the number of samples
     *                    actually written will always be nb_samples.
     */
    int write(void **data, int nb_samples);

    /**
     * Read data from the AVAudioFifo.
     *
     * @param data        audio data plane pointers
     * @param nb_samples  number of samples to read
     * @return            number of samples actually read, or negative AVERROR code
     *                    on failure. The number of samples actually read will not
     *                    be greater than nb_samples, and will only be less than
     *                    nb_samples if av_audio_fifo_size is less than nb_samples.
     */
    int read(void **data, int nb_samples);

    //

    void clear();

private:
    mutable std::mutex mtx_{};

    AVAudioFifo *buffer_{};
};

#endif //! CAPTURER_AuDIO_FIFO_H