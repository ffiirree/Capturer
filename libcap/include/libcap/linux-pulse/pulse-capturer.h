#ifndef CAPTURER_PULSE_CAPTURER_H
#define CAPTURER_PULSE_CAPTURER_H

#include "libcap/decoder.h"
#include "libcap/ringvector.h"

extern "C" {
#include <pulse/pulseaudio.h>
}

class PulseCapturer final : public Producer<AVFrame>
{
public:
    ~PulseCapturer() override { reset(); }

    int open(const std::string&, std::map<std::string, std::string>) override;

    void reset() override { destroy(); };

    int run() override;

    int produce(AVFrame *, AVMediaType) override;

    bool empty(AVMediaType type) override
    {
        switch (type) {
        case AVMEDIA_TYPE_AUDIO: return buffer_.empty();
        default:                 return true;
        }
    }

    bool has(AVMediaType type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_AUDIO: return ready_;
        default:                 return false;
        }
    }

    std::string format_str(AVMediaType) const override;
    AVRational  time_base(AVMediaType) const override;

    std::vector<av::vformat_t> vformats() const override { return {}; }

    std::vector<av::aformat_t> aformats() const override { return { afmt }; }

private:
    static void pulse_stream_read_callback(pa_stream *, size_t, void *);

    int destroy();

    RingVector<AVFrame *, 32> buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };

    av::frame frame_{};
    size_t    bytes_per_frame_{ 1 };
    size_t    frame_number_{ 0 };

    // pulse audio @{
    pa_stream *stream_{};
    // @}
};

#endif //! CAPTURER_PULSE_CAPTURER_H
