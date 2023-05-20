#ifdef __linux__

#include "linux-v4l2.h"

#include "probe/defer.h"
#include "logging.h"

#include <dirent.h>
#include <fcntl.h>
#include <libv4l2.h>

namespace v4l2
{
    std::vector<av::device_t> device_list()
    {
        std::vector<av::device_t> list;

        DIR *v4l2_dir = ::opendir("/sys/class/video4linux");
        if (nullptr == v4l2_dir) {
            LOG(ERROR) << "failed to open '/sys/class/video4linux'";
            return {};
        }
        defer(::closedir(v4l2_dir));

        struct dirent *next_dir_entry = nullptr;
        while ((next_dir_entry = ::readdir(v4l2_dir)) != nullptr) {
            if (next_dir_entry->d_type == DT_DIR) {
                continue;
            }

            std::string device_id = "/dev/" + std::string(next_dir_entry->d_name);

            int fd = -1;
            if ((fd = ::v4l2_open(device_id.c_str(), O_RDWR /* required */ | O_NONBLOCK)) == -1) {
                LOG(INFO) << "Cannot open " << device_id;
                continue;
            }
            defer(::v4l2_close(fd));

            v4l2_capability v4l2_cap{};
            if (::v4l2_ioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap) == -1) {
                LOG(INFO) << "Failed to query capabilities for " << device_id;
                continue;
            }

            auto caps = (v4l2_cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? v4l2_cap.device_caps
                                                                       : v4l2_cap.capabilities;
            if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
                DLOG(INFO) << device_id << " seems to not support video capture";
                continue;
            }

            DLOG(INFO) << fmt::format("Found device '{}' at {}",
                                      reinterpret_cast<const char *>(v4l2_cap.card), device_id);

            list.push_back(av::device_t{
                .name   = reinterpret_cast<char *>(v4l2_cap.card),
                .id     = device_id,
                .bus    = reinterpret_cast<char *>(v4l2_cap.bus_info),
                .type   = av::device_type_t::video | av::device_type_t::source,
                .format = av::device_format_t::v4l2,
            });
        }

        return list;
    }

    int open(const std::string& id) { return ::v4l2_open(id.c_str(), O_RDWR /* required */ | O_NONBLOCK); }

    void close(int dev) { ::v4l2_close(dev); }

    void input_list(int device)
    {
        v4l2_input input{};
        while (v4l2_ioctl(device, VIDIOC_ENUMINPUT, &input) == 0) {
            LOG(INFO) << fmt::format("fd {}: found input '{}' (Index {})", device,
                                     reinterpret_cast<const char *>(input.name), input.index);
            input.index++;
        }
    }

    void format_list(int device)
    {
        v4l2_fmtdesc fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while (v4l2_ioctl(device, VIDIOC_ENUM_FMT, &fmt) == 0) {
            std::string pix_fmt = reinterpret_cast<const char *>(fmt.description);
            if (fmt.flags & V4L2_FMT_FLAG_EMULATED) pix_fmt += " (Emulated)";

            LOG(INFO) << fmt::format("fd {} : {} is available", device, pix_fmt);

            resolution_list(device, fmt.pixelformat);

            fmt.index++;
        }
    }

    void standard_list(int device)
    {
        v4l2_standard std{};

        while (v4l2_ioctl(device, VIDIOC_ENUMSTD, &std) == 0) {
            LOG(INFO) << fmt::format("fd {} : {}, {}", device, reinterpret_cast<const char *>(std.name),
                                     std.id);
            std.index++;
        }
    }

    void resolution_list(int device, uint32_t pix_fmt)
    {
        v4l2_frmsizeenum frmsize{};
        frmsize.pixel_format = pix_fmt;

        v4l2_ioctl(device, VIDIOC_ENUM_FRAMESIZES, &frmsize);

        switch (frmsize.type) {
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            while (v4l2_ioctl(device, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
                LOG(INFO) << fmt::format("\tresolution={}x{}", frmsize.discrete.width,
                                         frmsize.discrete.height);
                framerate_list(device, pix_fmt, frmsize.discrete.width, frmsize.discrete.height);
                frmsize.index++;
            }

            break;

        default: LOG(INFO) << "unsupported"; break;
        }
    }

    void framerate_list(int device, uint32_t pix_fmt, uint32_t w, uint32_t h)
    {
        v4l2_frmivalenum frmival{};
        frmival.width        = w;
        frmival.height       = h;
        frmival.pixel_format = pix_fmt;

        v4l2_ioctl(device, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);

        switch (frmival.type) {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
            while (v4l2_ioctl(device, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                LOG(INFO) << fmt::format("\t\tframerate: {}/{}", frmival.discrete.denominator,
                                         frmival.discrete.numerator);
                frmival.index++;
            }
            break;

        default: LOG(ERROR) << "unsupported"; break;
        }
    }

    static void v4l2_update_controls_menu(uint32_t device, struct v4l2_queryctrl *qctrl)
    {
        v4l2_querymenu qmenu{};
        qmenu.id = qctrl->id;

        for (qmenu.index = qctrl->minimum; qmenu.index <= (uint32_t)qctrl->maximum;
             qmenu.index += qctrl->step) {
            if (v4l2_ioctl(device, VIDIOC_QUERYMENU, &qmenu) == 0) {
                LOG(INFO) << fmt::format("\t\t name = {}, index = {}",
                                         reinterpret_cast<const char *>(qmenu.name), (uint32_t)qmenu.index);
            }
        }
    }

    void controls(uint32_t device)
    {
        v4l2_queryctrl qctrl{};
        qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

        while (v4l2_ioctl(device, VIDIOC_QUERYCTRL, &qctrl) == 0) {
            switch (qctrl.type) {
            case V4L2_CTRL_TYPE_INTEGER:
                LOG(INFO) << fmt::format(
                    "V4L2_CTRL_TYPE_INTEGER : name = {}, min = {}, max = {}, step = {}, default = {}",
                    reinterpret_cast<const char *>(qctrl.name), qctrl.minimum, qctrl.maximum, qctrl.step,
                    qctrl.default_value);
                break;
            case V4L2_CTRL_TYPE_BOOLEAN:
                LOG(INFO) << fmt::format("V4L2_CTRL_TYPE_BOOLEAN : name = {}, default = {}",
                                         reinterpret_cast<const char *>(qctrl.name), qctrl.default_value);
                break;
            case V4L2_CTRL_TYPE_MENU:
            case V4L2_CTRL_TYPE_INTEGER_MENU:
                LOG(INFO) << "V4L2_CTRL_TYPE_MENU : name = " << qctrl.name
                          << ", default index = " << qctrl.default_value;
                v4l2_update_controls_menu(device, &qctrl);
                break;
            }
            qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
    }
} // namespace v4l2

#endif //! __linux__