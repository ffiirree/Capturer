#ifndef CAPTURER_MFVC_CAPTURER_H
#define CAPTURER_MFVC_CAPTURER_H

#ifdef _WIN32

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"

#include <mfidl.h>
#include <mfreadwrite.h>
#include <winrt/base.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

// https://learn.microsoft.com/en-us/windows/win32/medfound/audio-video-capture
// https://learn.microsoft.com/en-us/windows/win32/medfound/using-the-source-reader-in-asynchronous-mode
class MFCameraCapturer final : public Producer<av::frame>
{
public:
    ~MFCameraCapturer() override;

    int open(const std::string& device_id, std::map<std::string, std::string> options) override;

    int start() override;

    void stop() override;

    [[nodiscard]] bool has(AVMediaType type) const override;

    [[nodiscard]] bool is_realtime() const override;

    [[nodiscard]] std::vector<av::vformat_t> video_formats() const override;

private:
    void handler(IMFSample *Sample);
    int  video_decode(const av::packet& pkt);

    winrt::com_ptr<IMFMediaSource>  source_{};
    winrt::com_ptr<IMFSourceReader> reader_{};

    AVCodecContext *vcodec_ctx_{};

    av::frame frame_{};
};

#endif

#endif //! CAPTURER_MFVC_CAPTURER_H