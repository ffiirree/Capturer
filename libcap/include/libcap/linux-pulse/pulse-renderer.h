#ifndef CAPTURER_PULSE_RENDER_H
#define CAPTURER_PULSE_RENDER_H

#include "libcap/audio-renderer.h"
#include "libcap/devices.h"

extern "C" {
#include <pulse/pulseaudio.h>
}

class PulseAudioRenderer final : public AudioRenderer
{
public:
    int open(const std::string& name, RenderFlags flags) override;
    [[nodiscard]] bool ready() const override { return ready_; }

    int start() override;
    int reset() override;
    int stop() override;

    int mute(bool) override;
    [[nodiscard]] bool muted() const override;

    // 0.0 ~ 1.0
    int setVolume(float) override;
    [[nodiscard]] float volume() const override;

    int pause() override;
    int resume() override;
    [[nodiscard]] bool paused() const override;

    [[nodiscard]] av::aformat_t format() const override { return format_; }

    // sample number
    [[nodiscard]] uint32_t buffer_size() const override { return buffer_frames_; }

private:
    static void pulse_stream_success_callback(pa_stream *, int success, void *);
    static void pulse_stream_write_callback(pa_stream *, size_t, void *);
    static void pulse_stream_state_callback(pa_stream *, void *);
    static void pulse_stream_latency_callback(pa_stream *, void *);
    static void pulse_stream_underflow_callback(pa_stream *, void *);
    static void pulse_stream_update_timing_callback(pa_stream *, int, void *);
    static void pulse_stream_drain_callback(pa_stream *, int, void *);

    av::aformat_t format_{};
    av::device_t devinfo_{};

    uint32_t buffer_frames_{ 0 };
    uint32_t bytes_per_frame_{ 1 };

    // pulse audio @{
    pa_stream *stream_{};
    std::atomic<bool> ready_{ false };
    std::atomic<bool> stream_ready_{ false };
    std::atomic<int> stream_retval_{};
    // @}
};

#endif //! CAPTURER_PULSE_RENDER_H