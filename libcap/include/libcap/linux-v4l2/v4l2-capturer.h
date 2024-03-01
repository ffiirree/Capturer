#ifndef CAPTURER_LINUX_V4L2_CAPTURER_H
#define CAPTURER_LINUX_V4L2_CAPTURER_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"

class V4l2Capturer final : public Producer<av::frame>
{
public:
    V4l2Capturer() = default;

    ~V4l2Capturer() override;

    int open(const std::string& device_id, std::map<std::string, std::string> options) override;

    int start() override;

    void stop() override;

    [[nodiscard]] bool has(AVMediaType type) const override;

    [[nodiscard]] bool is_realtime() const override { return true; }

    [[nodiscard]] std::vector<av::vformat_t> video_formats() const override { return { vfmt }; }

private:
    std::string name_{};
    int         fd_{ -1 };
    int         input_{ -1 };
    uint32_t    caps_{ 0 };
    uint32_t    linesize_{ 0 }; // bytes per line
    int         timing_{ -1 };
    int         standard_{ -1 };

    std::jthread thread_{};
    size_t       frame_number_{};

private:
    int             v4l2_start_capture();
    int             video_decode(const av::packet& pkt);
    AVCodecContext *vcodec_ctx_{};
    av::frame       frame_{};

private:
    struct v4l2_buffer_mapping
    {
        size_t size{};
        void  *data{};
    };

    std::vector<v4l2_buffer_mapping> buf_mappings_{};
};

#endif //! CAPTURER_LINUX_V4L2_CAPTURER_H