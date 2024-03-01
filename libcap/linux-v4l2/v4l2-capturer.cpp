#include "libcap/linux-v4l2/v4l2-capturer.h"

#ifdef __linux__

#include "libcap/linux-v4l2/linux-v4l2.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <libv4l2.h>
extern "C" {
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/select.h>
}

int V4l2Capturer::open(const std::string& device_id, std::map<std::string, std::string>)
{
    name_ = device_id;

    if (fd_ = ::v4l2_open(device_id.c_str(), O_RDWR | O_NONBLOCK); fd_ == -1) {
        LOG(ERROR) << "[       V4L2] failed to open the device: " << device_id;
        return -1;
    }

    if (::v4l2_ioctl(fd_, VIDIOC_G_INPUT, &input_) < 0) {
        LOG(ERROR) << "[       V4L2] unable to get input : " << input_;
        return -1;
    }

    LOG(INFO) << "[       V4L2] Input : " << input_;

    v4l2_input in{};
    in.index = input_;
    if (::v4l2_ioctl(fd_, VIDIOC_ENUMINPUT, &in) < 0) {
        LOG(ERROR) << "[       V4L2] failed to get input capabilities";
        return -1;
    }
    caps_ = in.capabilities;

    if (caps_ & V4L2_IN_CAP_STD) {
        if (::v4l2_ioctl(fd_, VIDIOC_G_STD, &standard_) < 0) {
            LOG(ERROR) << "[       V4L2] failed to get input standard";

            return -1;
        }
    }

    if ((caps_ & V4L2_IN_CAP_DV_TIMINGS) && (timing_ > -1)) {
#if defined(VIDIOC_ENUM_DV_TIMINGS) && defined(V4L2_IN_CAP_DV_TIMINGS)
        v4l2_enum_dv_timings timings{};
        timings.index = timing_;
        if (::v4l2_ioctl(fd_, VIDIOC_ENUM_DV_TIMINGS, &timings) >= 0) {
            if (::v4l2_ioctl(fd_, VIDIOC_S_DV_TIMINGS, &timings.timings) < 0) {
                LOG(ERROR) << "[       V4L2] failed to set dv timings";

                return -1;
            }
        }
#endif
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::v4l2_ioctl(fd_, VIDIOC_G_FMT, &fmt) < 0) {
        return -1;
    }

    // pixel format
    const auto& [codec_id, pix_fmt] = v4l2::to_ffmpeg_format(fmt.fmt.pix.pixelformat);
    if (codec_id == AV_CODEC_ID_NONE && pix_fmt == AV_PIX_FMT_NONE) {
        LOG(ERROR) << "[       V4L2] unsupported pixel format";

        return -1;
    }
    vfmt.width   = static_cast<int>(fmt.fmt.pix.width);
    vfmt.height  = static_cast<int>(fmt.fmt.pix.height);
    vfmt.pix_fmt = pix_fmt;
    linesize_    = fmt.fmt.pix.bytesperline;

    // framerate
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::v4l2_ioctl(fd_, VIDIOC_G_PARM, &parm) < 0) {
        LOG(ERROR) << "[       V4L2] failed to get stream paramters";
        return -1;
    }
    vfmt.framerate = {
        static_cast<int>(parm.parm.capture.timeperframe.numerator),
        static_cast<int>(parm.parm.capture.timeperframe.denominator),
    };
    vfmt.time_base = OS_TIME_BASE_Q;

    // map buffers
    v4l2_requestbuffers req{};
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (::v4l2_ioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        LOG(ERROR) << "[       V4L2] failed to request buffers";
        return -1;
    }

    v4l2_buffer buffer{};
    buffer.type   = req.type;
    buffer.memory = req.memory;

    for (buffer.index = 0; buffer.index < req.count; ++buffer.index) {
        if (::v4l2_ioctl(fd_, VIDIOC_QUERYBUF, &buffer) < 0) {
            LOG(ERROR) << "[       V4L2] failed to query buffer";
            return -1;
        }
        void *ptr =
            ::v4l2_mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buffer.m.offset);
        if (ptr == MAP_FAILED) {
            LOG(ERROR) << "[       V4L2] failed to mmap";
            return -1;
        }
        buf_mappings_.push_back({ buffer.length, ptr });
    }

    // decoder
    if (codec_id != AV_CODEC_ID_RAWVIDEO) {}

    ready_ = true;
    return 0;
}

int V4l2Capturer::start()
{
    if (!ready_ || running_) {
        LOG(ERROR) << "[       V4L2] not ready or already running";
        return -1;
    }

    v4l2_buffer qbuf{};
    qbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    qbuf.memory = V4L2_MEMORY_MMAP;

    for (qbuf.index = 0; qbuf.index < buf_mappings_.size(); ++qbuf.index) {
        if (::v4l2_ioctl(fd_, VIDIOC_QBUF, &qbuf) < 0) {
            LOG(ERROR) << "[       V4L2] unable to queue buffer";
            return -1;
        }
    }

    constexpr v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::v4l2_ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        LOG(ERROR) << "[       V4L2] failed to start stream";
        return -1;
    }

    running_ = true;
    thread_  = std::jthread([this] {
        probe::thread::set_name("V4L2-" + name_);

        fd_set      fds{};
        timeval     tv{};
        v4l2_buffer buffer{};
        av::frame   frame{};

        while (running_) {
            FD_ZERO(&fds);
            FD_SET(fd_, &fds);

            tv = { .tv_sec = 0, .tv_usec = (1000000 * 5 * vfmt.framerate.den) / vfmt.framerate.num };
            const auto ret = ::select(fd_ + 1, &fds, nullptr, nullptr, &tv);
            if (ret < 0) {
                if (errno == EINTR) continue;

                LOG(ERROR) << name_ << ": select failed";
                running_ = false;
            }
            else if (ret == 0) {
                LOG(WARNING) << name_ << ": select timeout";

                if (v4l2_ioctl(fd_, VIDIOC_LOG_STATUS) < 0) {
                    LOG(ERROR) << name_ << ": failed to log status";
                }

                continue;
            }

            buffer.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;

            if (::v4l2_ioctl(fd_, VIDIOC_DQBUF, &buffer) < 0) {
                if (errno == EAGAIN) {
                    continue;
                }
                LOG(ERROR) << name_ << ": failed to dequeue buffer";
                running_ = false;
            }

            // output
            frame.unref();
            frame->width  = vfmt.width;
            frame->height = vfmt.height;
            frame->format = vfmt.pix_fmt;
            frame->pts    = av::clock::ns().count();
            const auto x  = av_frame_get_buffer(frame.get(), 0);

            void *ptr = buf_mappings_[buffer.index].data;
            for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
                for (int j = 0; j < frame->height; ++j) {
                    std::memcpy(frame->data[i] + j * frame->linesize[i], ptr + j * frame->linesize[i],
                                 frame->linesize[i]);
                }
            }

            DLOG(INFO) << fmt::format(
                "[V]  frame = {:>5d}, pts = {:>14d}, size = {:>4d}x{:>4d}, ts={:.3%T}", frame_number_++,
                frame->pts, frame->width, frame->height, std::chrono::nanoseconds{ frame->pts });

            onarrived(frame, AVMEDIA_TYPE_VIDEO);

            if (::v4l2_ioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
                LOG(ERROR) << name_ << ": failed to dequeue buffer";
                running_ = false;
            }
        }
    });
    return 0;
}

bool V4l2Capturer::has(const AVMediaType type) const { return type == AVMEDIA_TYPE_VIDEO; }

void V4l2Capturer::stop()
{
    running_ = false;
    ready_   = false;

    constexpr v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::v4l2_ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) {
        LOG(ERROR) << "[       V4L2] failed to stop stream";
    }

    if (thread_.joinable()) thread_.join();
}

V4l2Capturer::~V4l2Capturer()
{
    stop();

    for (const auto& [size, data] : buf_mappings_) {
        if (data != MAP_FAILED && data != nullptr) {
            ::v4l2_munmap(data, size);
        }
    }

    if (fd_ != -1) v4l2_close(fd_);
}

#endif