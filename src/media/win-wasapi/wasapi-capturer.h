#ifndef CAPTURER_WASAPI_CAPTURER_H
#define CAPTURER_WASAPI_CAPTURER_H

#ifdef _WIN32

#include <Windows.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <functiondiscoverykeys.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <optional>
#include <Audioclient.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#include "ringvector.h"
#include "producer.h"
#include "clock.h"

enum class DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_SPEAKER,
    DEVICE_MICROPHONE
};

class WasapiCapturer : public Producer<AVFrame> {
public:
    ~WasapiCapturer() override { reset(); }

    int open(DeviceType);

    void reset() override { destroy(); };

    int run() override;

    int produce(AVFrame*, int) override;

    bool empty(int type) override
    {
        switch (type)
        {
        case AVMEDIA_TYPE_AUDIO: return buffer_.empty();
        default: return true;
        }
    }

    bool has(int type) const override
    {
        switch (type)
        {
        case AVMEDIA_TYPE_AUDIO: return ready_;
        default: return false;
        }
    }
    std::string format_str(int) const override;
    AVRational time_base(int) const override;

    void mute(bool v) { muted_ = v; }

private:
    int run_f();
    int destroy();

    static uint64_t to_ffmpeg_channel_layout(DWORD layout, int channels);

    void init_format(WAVEFORMATEX*);

    int init_capturer(IMMDevice*);
    // play a silent to fix silent loopback
    int init_silent_render(IMMDevice*);

    int process_received_data(BYTE*, UINT32, UINT64);

    RingVector<AVFrame*, 32> buffer_{
            []() { return av_frame_alloc(); },
            [](AVFrame** frame) { av_frame_free(frame); }
    };

    int64_t start_time_{ AV_NOPTS_VALUE };

    std::atomic<bool> muted_{ false };

    DeviceType type_{ DeviceType::DEVICE_UNKNOWN };

    AVFrame* frame_{ nullptr };
    uint32_t frame_number_{ 0 };

    // audio params @{
    int sample_rate_{ 44100 };
    int channels_{ 2 };
    int bit_rate_{ 0 };
    enum AVSampleFormat sample_fmt_ { AV_SAMPLE_FMT_FLT };
    uint64_t channel_layout_{ 0 };
    AVRational time_base_{ 1, OS_TIME_BASE };
    // @}

    // WASAPI Capturer@{
    IAudioClient* capturer_audio_client_{ nullptr };
    IAudioCaptureClient* capturer_{ nullptr };
    // @}

    // WASAPI Render@{
    IAudioClient* render_audio_client_{ nullptr };
    IAudioRenderClient* render_{ nullptr };
    // @}

    // events
    HANDLE AUDIO_SAMPLES_READY_EVENT{ nullptr };
    HANDLE STOP_EVENT{ nullptr };
};
#endif // _WIN32

#endif // !CAPTURER_WASAPI_CAPTURER_H
