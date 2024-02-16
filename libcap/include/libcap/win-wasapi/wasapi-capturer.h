#ifndef CAPTURER_WASAPI_CAPTURER_H
#define CAPTURER_WASAPI_CAPTURER_H

#ifdef _WIN32

#include "libcap/devices.h"
#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"
#include "libcap/queue.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <Windows.h>
#include <winrt/base.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class WasapiCapturer : public Producer<av::frame>
{
public:
    ~WasapiCapturer() override { reset(); }

    int open(const std::string&, std::map<std::string, std::string>) override;

    void reset() override { destroy(); };

    int run() override;

    int produce(av::frame&, AVMediaType) override;

    bool empty(AVMediaType type) override
    {
        switch (type) {
        case AVMEDIA_TYPE_AUDIO: return buffer_.empty();
        default:                 return true;
        }
    }

    bool has(AVMediaType type) const override { return type == AVMEDIA_TYPE_AUDIO; }

    std::string format_str(AVMediaType) const override;
    AVRational  time_base(AVMediaType) const override;

    std::vector<av::vformat_t> vformats() const override { return {}; }

    std::vector<av::aformat_t> aformats() const override { return { afmt }; }

private:
    int destroy();

    void init_format(WAVEFORMATEX *);

    int init_capturer(IMMDevice *);
    // play a silent to fix silent loopback
    int init_silent_render(IMMDevice *);

    int process_received_data(BYTE *, UINT32, UINT64);

    std::jthread thread_{};

    safe_queue<av::frame> buffer_{ 32 };

    int64_t start_time_{ AV_NOPTS_VALUE };

    uint32_t frame_number_{ 0 };

    // WASAPI Capturer@{
    winrt::com_ptr<IAudioClient>        capturer_audio_client_{};
    winrt::com_ptr<IAudioCaptureClient> capturer_{};
    // @}

    // WASAPI Render@{
    winrt::com_ptr<IAudioClient>       render_audio_client_{};
    winrt::com_ptr<IAudioRenderClient> render_{};
    // @}

    // events
    HANDLE ARRIVED_EVENT{ nullptr };
    HANDLE STOP_EVENT{ nullptr };

    av::device_t device_{};
};
#endif // _WIN32

#endif //! CAPTURER_WASAPI_CAPTURER_H
