#ifndef CAPTURER_CAMERA_CAPTURER_H
#define CAPTURER_CAMERA_CAPTURER_H

#ifdef _WIN32

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"

#include <dshow.h>
#include <winrt/base.h>

class DShowCameraCapturer final : public Producer<av::frame>
{
public:
    int open(const std::string&, std::map<std::string, std::string>) override;

    int start() override;

    bool has(AVMediaType) const override;

    void enable(AVMediaType t, bool value) override;

    bool is_realtime() const override { return true; }

private:
    winrt::com_ptr<IGraphBuilder>         graph_{};
    winrt::com_ptr<ICaptureGraphBuilder2> builder_{};
    winrt::com_ptr<IMediaControl>         control_{};
    winrt::com_ptr<IBaseFilter>           filter_;

    std::atomic<bool> enabled_{ true };
};

#endif

#endif //! CAPTURER_CAMERA_CAPTURER_H