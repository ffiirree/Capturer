#ifdef __linux__

#include "libcap/linux-v4l2/linux-v4l2.h"

#include "logging.h"
#include "probe/defer.h"

#include <fmt/format.h>
#include <libv4l2.h>
extern "C" {
#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
}

namespace v4l2
{
    std::vector<av::device_t> device_list()
    {
        std::vector<av::device_t> list{};

#ifdef __FreeBSD__
        constexpr auto dir = "/dev";
#else
        constexpr auto dir = "/sys/class/video4linux";
#endif
        DIR *v4l2_dir = ::opendir(dir);

        if (nullptr == v4l2_dir) {
            LOG(ERROR) << "[       V4L2] failed to open dir '" << dir << "'";
            return {};
        }
        defer(::closedir(v4l2_dir));

        dirent *dentry = nullptr;
        while ((dentry = ::readdir(v4l2_dir)) != nullptr) {
#ifdef __FreeBSD__
            if (::strstr(dentry->d_name, "video") == nullptr) continue;
#endif

            if (dentry->d_type == DT_DIR) continue;

            std::string device_id = "/dev/" + std::string(dentry->d_name);
            DLOG(INFO) << "[       V4L2] check " << device_id;

            int fd = -1;
            if ((fd = ::v4l2_open(device_id.c_str(), O_RDWR /* required */ | O_NONBLOCK)) == -1) {
                DLOG(INFO) << "[       V4L2] cannot open camera " << device_id;
                continue;
            }
            defer(::v4l2_close(fd));

            v4l2_capability v4l2_cap{};
            if (::v4l2_ioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap) == -1) {
                DLOG(INFO) << "[       V4L2] failed to query capabilities for " << device_id;
                continue;
            }
#ifndef V4L2_CAP_DEVICE_CAPS
            const auto caps = v4l2_cap.capabilities;
#else
            const auto caps = (v4l2_cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? v4l2_cap.device_caps
                                                                             : v4l2_cap.capabilities;
#endif

            if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
                DLOG(INFO) << "[       V4L2] " << device_id << " seems to not support video capture";
                continue;
            }

            DLOG(INFO) << fmt::format("[       V4L2] found device '{} - {}' at {}",
                                      reinterpret_cast<const char *>(v4l2_cap.card),
                                      reinterpret_cast<const char *>(v4l2_cap.bus_info), device_id);

            list.push_back(av::device_t{
                .name   = reinterpret_cast<char *>(v4l2_cap.card),
                .id     = device_id,
                .bus    = reinterpret_cast<char *>(v4l2_cap.bus_info),
                .type   = av::device_type_t::video | av::device_type_t::source,
                .format = av::device_format_t::V4L2,
            });
        }

        return list;
    }

    std::pair<AVCodecID, AVPixelFormat> to_ffmpeg_format(const uint32_t fmt)
    {
        switch (fmt) {
        case V4L2_PIX_FMT_RGB555:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB555LE };
        case V4L2_PIX_FMT_RGB565:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB565LE };
        case V4L2_PIX_FMT_RGB555X: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB555BE };
        case V4L2_PIX_FMT_RGB565X: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB565BE };
        case V4L2_PIX_FMT_BGR24:   return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BGR24 };
        case V4L2_PIX_FMT_RGB24:   return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB24 };
        case V4L2_PIX_FMT_ABGR32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BGRA };
        case V4L2_PIX_FMT_BGRA32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_ABGR };
        case V4L2_PIX_FMT_ARGB32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_ARGB };
        case V4L2_PIX_FMT_RGBA32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGBA };
        case V4L2_PIX_FMT_BGR32:   return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BGR0 };
        case V4L2_PIX_FMT_RGB32:   return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_0RGB };
        case V4L2_PIX_FMT_XBGR32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BGR0 };
        case V4L2_PIX_FMT_BGRX32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_0BGR };
        case V4L2_PIX_FMT_XRGB32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_0RGB };
        case V4L2_PIX_FMT_RGBX32:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB0 };
        case V4L2_PIX_FMT_GREY:    return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY8 };
        case V4L2_PIX_FMT_Y16:     return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY16LE };
        case V4L2_PIX_FMT_Y16_BE:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY16BE };
        case V4L2_PIX_FMT_PAL8:    return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_PAL8 };
        case V4L2_PIX_FMT_YUYV:    return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUYV422 };
        case V4L2_PIX_FMT_UYVY:    return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_UYVY422 };
        case V4L2_PIX_FMT_NV12:    return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_NV12 };
        case V4L2_PIX_FMT_YUV410:
        case V4L2_PIX_FMT_YVU410:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUV410P };
        case V4L2_PIX_FMT_YUV411P: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUV411P };
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YVU420:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUV420P };
        case V4L2_PIX_FMT_YUV422P: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUV422P };
        case V4L2_PIX_FMT_SBGGR8:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BAYER_BGGR8 };
        case V4L2_PIX_FMT_SGBRG8:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BAYER_GBRG8 };
        case V4L2_PIX_FMT_SGRBG8:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BAYER_GRBG8 };
        case V4L2_PIX_FMT_SRGGB8:  return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_BAYER_RGGB8 };
        case V4L2_PIX_FMT_Z16:     return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY16LE };
        case V4L2_PIX_FMT_MJPEG:
        case V4L2_PIX_FMT_JPEG:    return { AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE };
        case V4L2_PIX_FMT_H264:    return { AV_CODEC_ID_H264, AV_PIX_FMT_NONE };
        case V4L2_PIX_FMT_MPEG4:   return { AV_CODEC_ID_MPEG4, AV_PIX_FMT_NONE };
        case V4L2_PIX_FMT_VP8:     return { AV_CODEC_ID_VP8, AV_PIX_FMT_NONE };
        case V4L2_PIX_FMT_VP9:     return { AV_CODEC_ID_VP9, AV_PIX_FMT_NONE };
        case V4L2_PIX_FMT_HEVC:    return { AV_CODEC_ID_HEVC, AV_PIX_FMT_NONE };
        default:                   return { AV_CODEC_ID_NONE, AV_PIX_FMT_NONE };
        }
    }

    namespace properties
    {
        std::vector<v4l2_input> inputs(int fd)
        {
            std::vector<v4l2_input> list{};

            v4l2_input input{};
            while (::v4l2_ioctl(fd, VIDIOC_ENUMINPUT, &input) == 0) {
                DLOG(INFO) << fmt::format("[ V4L2-INPUT] {}: found input '{}' (Index {})", fd,
                                          reinterpret_cast<const char *>(input.name), input.index);
                list.push_back(input);
                input.index++;
            }

            return list;
        }

        std::vector<v4l2_standard> standards(int fd)
        {
            std::vector<v4l2_standard> list{};

            v4l2_standard std{};
            while (::v4l2_ioctl(fd, VIDIOC_ENUMSTD, &std) == 0) {
                DLOG(INFO) << fmt::format("[   V4L2-STD] fd {} : {}, {}", fd,
                                          reinterpret_cast<const char *>(std.name), std.id);
                list.push_back(std);
                std.index++;
            }

            return list;
        }

        std::vector<v4l2_fmtdesc> formats(int fd)
        {
            std::vector<v4l2_fmtdesc> list{};

            v4l2_fmtdesc fmt{};
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            while (::v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
                std::string pix_fmt = reinterpret_cast<const char *>(fmt.description);
                if (fmt.flags & V4L2_FMT_FLAG_EMULATED) pix_fmt += " (Emulated)";

                DLOG(INFO) << fmt::format("[V4L2-PIXFMT] {}: found {}", fd, pix_fmt);

                list.push_back(fmt);
                fmt.index++;
            }

            return list;
        }

        std::vector<v4l2_frmsizeenum> resolutions(const int fd, const uint32_t pix_fmt)
        {
            std::vector<v4l2_frmsizeenum> list{};

            v4l2_frmsizeenum frmsize{};
            frmsize.pixel_format = pix_fmt;

            ::v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);

            switch (frmsize.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                while (::v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {

                    DLOG(INFO) << fmt::format("[  V4L2-SIZE] {}: {} x {}", fd, frmsize.discrete.width,
                                              frmsize.discrete.height);

                    list.push_back(frmsize);
                    frmsize.index++;
                }

                break;

            default: LOG(ERROR) << "unsupported"; break;
            }

            return list;
        }

        std::vector<v4l2_frmivalenum> framerates(const int fd, const uint32_t pix_fmt, const uint32_t w,
                                                 const uint32_t h)
        {
            std::vector<v4l2_frmivalenum> list{};

            v4l2_frmivalenum frmival{};
            frmival.width        = w;
            frmival.height       = h;
            frmival.pixel_format = pix_fmt;

            ::v4l2_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);

            switch (frmival.type) {
            case V4L2_FRMIVAL_TYPE_DISCRETE:
                while (v4l2_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                    DLOG(INFO) << fmt::format("[   V4L2-FPS] \t\t{}/{}", frmival.discrete.denominator,
                                              frmival.discrete.numerator);
                    list.push_back(frmival);
                    frmival.index++;
                }
                break;

            default: LOG(ERROR) << "unsupported"; break;
            }
            return list;
        }

        std::vector<v4l2_dv_timings> timings(const int fd)
        {
            std::vector<v4l2_dv_timings> list{};

            v4l2_enum_dv_timings iter{};
            for (iter.index = 0; ::v4l2_ioctl(fd, VIDIOC_ENUM_DV_TIMINGS, &iter) == 0; ++iter.index) {
                list.push_back(iter.timings);
            }

            return list;
        }
    } // namespace properties

    namespace ctrl
    {
        std::vector<control> controls(const int fd)
        {
            std::vector<control> list{};

            v4l2_queryctrl qctrl{};
            for (qctrl.id  = V4L2_CTRL_FLAG_NEXT_CTRL; ::v4l2_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0;
                 qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL) {
                switch (qctrl.type) {
                case V4L2_CTRL_TYPE_INTEGER:
                    list.push_back({
                        .id            = qctrl.id,
                        .type          = qctrl.type,
                        .name          = reinterpret_cast<const char *>(qctrl.name),
                        .minimum       = qctrl.minimum,
                        .maximum       = qctrl.maximum,
                        .value         = get(fd, qctrl.id).value_or(qctrl.default_value),
                        .default_value = qctrl.default_value,
                    });
                    break;

                case V4L2_CTRL_TYPE_BOOLEAN:
                    list.push_back({
                        .id            = qctrl.id,
                        .type          = qctrl.type,
                        .name          = reinterpret_cast<const char *>(qctrl.name),
                        .minimum       = qctrl.minimum,
                        .maximum       = qctrl.maximum,
                        .value         = get(fd, qctrl.id).value_or(qctrl.default_value),
                        .default_value = qctrl.default_value,
                    });
                    break;

                case V4L2_CTRL_TYPE_MENU:
                case V4L2_CTRL_TYPE_INTEGER_MENU: {
                    v4l2_querymenu qmenu{};
                    qmenu.id = qctrl.id;

                    std::map<int32_t, std::string> options{};
                    for (qmenu.index  = qctrl.minimum; qmenu.index <= static_cast<uint32_t>(qctrl.maximum);
                         qmenu.index += qctrl.step) {
                        if (::v4l2_ioctl(fd, VIDIOC_QUERYMENU, &qmenu) == 0) {
                            options[static_cast<int32_t>(qmenu.index)] =
                                reinterpret_cast<const char *>(qmenu.name);
                        }
                    }

                    list.push_back({
                        .id            = qctrl.id,
                        .type          = qctrl.type,
                        .name          = reinterpret_cast<const char *>(qctrl.name),
                        .minimum       = qctrl.minimum,
                        .maximum       = qctrl.maximum,
                        .value         = get(fd, qctrl.id).value_or(qctrl.default_value),
                        .default_value = qctrl.default_value,
                        .options       = options,
                    });

                    break;
                }

                default: break;
                }
            }

            return list;
        }

        std::optional<int32_t> get(const int fd, const uint32_t id)
        {
            v4l2_control ctrl{};
            ctrl.id = id;
            if (::v4l2_ioctl(fd, VIDIOC_G_CTRL, &ctrl) != 0) return std::nullopt;

            return ctrl.value;
        }

        int set(const int fd, const uint32_t id, const int32_t value)
        {
            v4l2_control ctrl{ .id = id, .value = value };
            if (::v4l2_ioctl(fd, VIDIOC_S_CTRL, &ctrl) != 0) return -1;

            return 0;
        }
    } // namespace ctrl

} // namespace v4l2

#endif //! __linux__