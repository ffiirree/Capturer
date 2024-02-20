#ifndef CAPTURER_PULSE_CAPTURER_H
#define CAPTURER_PULSE_CAPTURER_H

#include "libcap/decoder.h"
#include "libcap/queue.h"

extern "C" {
#include <pulse/pulseaudio.h>
}

class PulseCapturer final : public Producer<av::frame>
{
public:
    PulseCapturer();

    ~PulseCapturer() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int start() override;

    void stop() override;

    bool has(AVMediaType type) const override;

    bool is_realtime() const override { return true; }

    std::vector<av::aformat_t> audio_formats() const override { return { afmt }; }

private:
    static void pulse_stream_read_callback(pa_stream *, size_t, void *);

    av::frame frame_{};
    size_t    bytes_per_frame_{ 1 };
    size_t    frame_number_{ 0 };

    // pulse audio @{
    pa_stream *stream_{};
    // @}
};

#endif //! CAPTURER_PULSE_CAPTURER_H
