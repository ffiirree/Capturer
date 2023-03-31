#ifdef __linux__

#include <libv4l2.h>
#include <fcntl.h>
#include <dirent.h>
#include "logging.h"
#include "linux-v4l2.h"
#include "defer.h"

std::vector<V4l2Device> v4l2_device_list()
{
    std::vector<V4l2Device> list;

    DIR *v4l2_dir = opendir("/sys/class/video4linux");
    if (nullptr == v4l2_dir) {
        LOG(ERROR) << "failed to open '/sys/class/video4linux'";
        return {};
    }
    defer(closedir(v4l2_dir));

    struct dirent *next_dir_entry = nullptr;
    while ((next_dir_entry = readdir(v4l2_dir)) != nullptr)
    {
        if (next_dir_entry->d_type == DT_DIR)
        {
            continue;
        }

        std::string device_id = "/dev/" + std::string(next_dir_entry->d_name);

        int fd = -1;
        if ((fd = v4l2_open(device_id.c_str(), O_RDWR /* required */ | O_NONBLOCK)) == -1)
        {
            LOG(INFO) << "Cannot open " << device_id;
            continue;
        }
        defer(v4l2_close(fd));

        struct v4l2_capability v4l2_cap = {0};
        if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap) == -1)
        {
            LOG(INFO) << "Failed to query capabilities for " << device_id;
            continue;
        }

        auto caps = (v4l2_cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? v4l2_cap.device_caps : v4l2_cap.capabilities;
        if (!(caps & V4L2_CAP_VIDEO_CAPTURE))
        {
            LOG(INFO) << device_id << " seems to not support video capture";
            continue;
        }

        LOG(INFO) << fmt::format("Found device '{}' at {}", reinterpret_cast<const char *>(v4l2_cap.card), device_id);

        list.emplace_back(device_id, v4l2_cap);
    }

    return list;
}

void v4l2_input_list(int device)
{
    struct v4l2_input input = {0};
    while (v4l2_ioctl(device, VIDIOC_ENUMINPUT, &input) == 0)
    {
        LOG(INFO) << fmt::format("fd {}: found input '{}' (Index {})",
                                 device, reinterpret_cast<const char *>(input.name), input.index);
        input.index++;
    }
}

void v4l2_format_list(int device)
{
    struct v4l2_fmtdesc fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (v4l2_ioctl(device, VIDIOC_ENUM_FMT, &fmt) == 0)
    {
        std::string pix_fmt = reinterpret_cast<const char *>(fmt.description);
        if (fmt.flags & V4L2_FMT_FLAG_EMULATED)
            pix_fmt += " (Emulated)";

        LOG(INFO) << fmt::format("fd {} : {} is available", device, pix_fmt);

        v4l2_resolution_list(device, fmt.pixelformat);

        fmt.index++;
    }
}

void v4l2_standard_list(int device)
{
    struct v4l2_standard std = {0};

    while (v4l2_ioctl(device, VIDIOC_ENUMSTD, &std) == 0)
    {
        LOG(INFO) << fmt::format("fd {} : {}, {}",
                                 device, reinterpret_cast<const char *>(std.name), std.id);
        std.index++;
    }
}

void v4l2_resolution_list(int device, uint32_t pix_fmt)
{
    struct v4l2_frmsizeenum frmsize = {0};
    frmsize.pixel_format = pix_fmt;

    v4l2_ioctl(device, VIDIOC_ENUM_FRAMESIZES, &frmsize);

    switch (frmsize.type)
    {
    case V4L2_FRMSIZE_TYPE_DISCRETE:
        while (v4l2_ioctl(device, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
        {
            LOG(INFO) << fmt::format("\tresolution={}x{}",
                                     frmsize.discrete.width, frmsize.discrete.height);
            v4l2_framerate_list(device, pix_fmt, frmsize.discrete.width, frmsize.discrete.height);
            frmsize.index++;
        }

        break;

    default:
        LOG(INFO) << "unsupported";
        break;
    }
}

void v4l2_framerate_list(int device, uint32_t pix_fmt, uint32_t w, uint32_t h)
{
    struct v4l2_frmivalenum frmival = {0};
    frmival.width = w;
    frmival.height = h;
    frmival.pixel_format = pix_fmt;

    v4l2_ioctl(device, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);

    switch (frmival.type)
    {
    case V4L2_FRMIVAL_TYPE_DISCRETE:
        while (v4l2_ioctl(device, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0)
        {
            LOG(INFO) << fmt::format("\t\tframerate: {}/{}", frmival.discrete.denominator, frmival.discrete.numerator);
            frmival.index++;
        }
        break;

    default:
        LOG(ERROR) << "unsupported";
        break;
    }
}

static void v4l2_update_controls_menu(uint32_t device, struct v4l2_queryctrl *qctrl)
{
    struct v4l2_querymenu qmenu = {0};
    qmenu.id = qctrl->id;

    for (qmenu.index = qctrl->minimum; qmenu.index <= (uint32_t)qctrl->maximum; qmenu.index += qctrl->step)
    {
        if (v4l2_ioctl(device, VIDIOC_QUERYMENU, &qmenu) == 0)
        {
            LOG(INFO) << fmt::format("\t\t name = {}, index = {}",
                                     reinterpret_cast<const char *>(qmenu.name), (uint32_t)qmenu.index);
        }
    }
}

void v4l2_controls(uint32_t device)
{
    struct v4l2_queryctrl qctrl = {0};
    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (v4l2_ioctl(device, VIDIOC_QUERYCTRL, &qctrl) == 0)
    {
        switch (qctrl.type)
        {
        case V4L2_CTRL_TYPE_INTEGER:
            LOG(INFO) << fmt::format("V4L2_CTRL_TYPE_INTEGER : name = {}, min = {}, max = {}, step = {}, default = {}",
                                     reinterpret_cast<const char *>(qctrl.name), qctrl.minimum, qctrl.maximum, qctrl.step, qctrl.default_value);
            break;
        case V4L2_CTRL_TYPE_BOOLEAN:
            LOG(INFO) << fmt::format("V4L2_CTRL_TYPE_BOOLEAN : name = {}, default = {}",
                                     reinterpret_cast<const char *>(qctrl.name), qctrl.default_value);
        case V4L2_CTRL_TYPE_MENU:
        case V4L2_CTRL_TYPE_INTEGER_MENU:
            LOG(INFO) << "V4L2_CTRL_TYPE_MENU : name = " << qctrl.name << ", default index = " << qctrl.default_value;
            v4l2_update_controls_menu(device, &qctrl);
            break;
        }
        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

#endif //! __linux__