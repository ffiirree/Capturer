#ifndef CAPTURER_LINUX_V4L2
#define CAPTURER_LINUX_V4L2

#ifdef __linux__

#include "devices.h"

#include <fmt/format.h>
#include <linux/videodev2.h>
#include <string>
#include <vector>

namespace v4l2
{
    std::vector<av::device_t> device_list();

    // open a v4l2 device by id, like /dev/vidoe0
    int open(const std::string& id);

    // close a v4l2 device
    void close(int dev);

    void input_list(int dev);

    void format_list(int dev);

    void standard_list(int dev);

    void resolution_list(int dev, uint32_t pix_fmt);

    void framerate_list(int dev, uint32_t pix_fmt, uint32_t w, uint32_t h);

    void controls(uint32_t dev);
} // namespace v4l2

#endif //! __linux__

#endif // !CAPTURER_LINUX_V4L2