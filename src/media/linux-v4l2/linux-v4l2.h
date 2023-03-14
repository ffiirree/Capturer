#ifndef CAPTURER_LINUX_V4L2
#define CAPTURER_LINUX_V4L2

#ifdef __linux__

#include <vector>
#include <string>
#include <linux/videodev2.h>
#include <fmt/format.h>

struct V4l2Device
{
    [[nodiscard]] std::string version_str() const
    {
        return fmt::format("{}.{}.{}",
                           (version_ >> 16) & 0xff, (version_ >> 8) & 0xff, version_ & 0xff);
    }

    V4l2Device(const std::string &name, const struct v4l2_capability &cap)
    {
        name_ = name;
        card_ = reinterpret_cast<const char *>(cap.card);
        driver_ = reinterpret_cast<const char *>(cap.driver);
        bus_info_ = reinterpret_cast<const char *>(cap.bus_info);
        version_ = cap.version;
        caps_ = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    }

    std::string name_{};
    std::string driver_{};
    std::string card_{};
    std::string bus_info_{};
    uint32_t version_{0};
    uint32_t caps_{0};
    int device_{-1};
};

std::vector<V4l2Device> v4l2_device_list();

void v4l2_input_list(int device);

void v4l2_format_list(int device);

void v4l2_standard_list(int device);

void v4l2_resolution_list(int device, uint32_t pix_fmt);

void v4l2_framerate_list(int device, uint32_t pix_fmt, uint32_t w, uint32_t h);

void v4l2_controls(uint32_t device);

#endif //! __linux__

#endif // !CAPTURER_LINUX_V4L2