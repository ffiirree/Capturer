#ifndef CAPTURER_WASAPI_CAPTURER_H
#define CAPTURER_WASAPI_CAPTURER_H

#ifdef _WIN32

#include "libcap/devices.h"
#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <Windows.h>
#include <winrt/base.h>

extern "C" {
#include <libavutil/avutil.h>
}

class WasapiCapturer final : public Producer<av::frame>
{
public:
    ~WasapiCapturer() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int start() override;

    void stop() override;

    bool has(const AVMediaType type) const override { return type == AVMEDIA_TYPE_AUDIO; }

    bool is_realtime() const override { return true; }

    std::vector<av::aformat_t> audio_formats() const override { return { afmt }; }

private:
    void InitWaveFormat(WAVEFORMATEX *);

    void InitCapturer(IMMDevice *);

    // play a silent audio to fix silent loopback
    void InitSlientRenderer(IMMDevice *);

    int ProcessReceivedData(BYTE *, UINT32, UINT64) const;

    std::jthread thread_{};

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
    winrt::handle ARRIVED_EVENT{};
    winrt::handle STOP_EVENT{};

    av::device_t device_{};
};
#endif // _WIN32

#endif //! CAPTURER_WASAPI_CAPTURER_H
