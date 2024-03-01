#ifndef CAPTURER_LINUX_V4L2_H
#define CAPTURER_LINUX_V4L2_H
#include <map>

#ifdef __linux__

#include "libcap/devices.h"

#include <vector>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
#include <linux/videodev2.h>
}

namespace v4l2
{
    std::vector<av::device_t> device_list();

    std::pair<AVCodecID, AVPixelFormat> to_ffmpeg_format(uint32_t);

    namespace properties
    {
        std::vector<v4l2_input> inputs(int fd);

        std::vector<v4l2_standard> standards(int fd);

        std::vector<v4l2_fmtdesc> formats(int fd);

        std::vector<v4l2_frmsizeenum> resolutions(int fd, uint32_t pix_fmt);

        std::vector<v4l2_frmivalenum> framerates(int fd, uint32_t pix_fmt, uint32_t w, uint32_t h);
    } // namespace properties

    namespace ctrl
    {
        struct control
        {
            uint32_t                       id{};
            uint32_t                       type{};
            std::string                    name{};
            int32_t                        minimum{};
            int32_t                        maximum{};
            int32_t                        value{};
            int32_t                        default_value{};
            std::map<int32_t, std::string> options{};
        };

        std::vector<control> controls(int fd);

        std::optional<int32_t> get(int fd, uint32_t id);
        int                    set(int fd, uint32_t id, int32_t value);
    } // namespace ctrl

} // namespace v4l2

#endif //! __linux__

#endif //! CAPTURER_LINUX_V4L2_H