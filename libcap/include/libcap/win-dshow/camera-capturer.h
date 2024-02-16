#ifndef CAPTURER_CAMERA_CAPTURER_H
#define CAPTURER_CAMERA_CAPTURER_H

#ifdef _WIN32

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"
#include "libcap/queue.h"

#include <dshow.h>
#include <winrt/base.h>

class DShowCameraCapturer final : public Producer<av::frame>
{
public:
    int open(const std::string&, std::map<std::string, std::string>) override;

    void reset() override;

    int run() override;

    int produce(av::frame&, AVMediaType) override;

    bool empty(AVMediaType) override;

    bool has(AVMediaType) const override;

    std::string format_str(AVMediaType) const override;

    AVRational time_base(AVMediaType) const override;

    bool enabled(AVMediaType t) override { return enabled_.contains(t) && enabled_[t]; }

    void enable(AVMediaType t) override { enabled_[t] = true; }

    // supported formats
    std::vector<av::vformat_t> vformats() const override;

private:
    void capture_fn();

    winrt::com_ptr<IGraphBuilder>         graph_{};
    winrt::com_ptr<ICaptureGraphBuilder2> builder_{};
    winrt::com_ptr<IMediaControl>         control_{};
    winrt::com_ptr<IBaseFilter>           filter_;

    safe_queue<av::frame> buffer_{};
};

#endif

#endif //! CAPTURER_CAMERA_CAPTURER_H