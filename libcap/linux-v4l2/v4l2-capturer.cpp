#include "libcap/linux-v4l2/v4l2-capturer.h"

#ifdef __linux__

#include "libcap/linux-v4l2/linux-v4l2.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <libv4l2.h>
#include <probe/defer.h>
extern "C" {
#include <fcntl.h>
#include <libavutil/imgutils.h>
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
    auto codec = avcodec_find_decoder(codec_id);
    if (vcodec_ctx_ = avcodec_alloc_context3(codec); !vcodec_ctx_) {
        LOG(ERROR) << "[       V4L2] failed to alloc decoder context";
        return -1;
    }

    vcodec_ctx_->codec_type  = AVMEDIA_TYPE_VIDEO;
    vcodec_ctx_->codec_id    = codec_id;
    vcodec_ctx_->pix_fmt     = vfmt.pix_fmt;
    vcodec_ctx_->width       = vfmt.width;
    vcodec_ctx_->height      = vfmt.height;
    vcodec_ctx_->flags2     |= AV_CODEC_FLAG2_FAST;
    if (vfmt.pix_fmt != AV_PIX_FMT_NONE)
        vcodec_ctx_->frame_size = av_image_get_buffer_size(vfmt.pix_fmt, vfmt.width, vfmt.height, 1);

    if (codec_id == AV_CODEC_ID_RAWVIDEO) {
        vcodec_ctx_->codec_tag = avcodec_pix_fmt_to_codec_tag(vfmt.pix_fmt);
    }

    if (avcodec_open2(vcodec_ctx_, codec, nullptr) < 0) {
        LOG(ERROR) << "[       V4L2] can not open the decoder";
        return -1;
    }

    ready_ = true;

    // probe pixel format
    if (codec_id != AV_CODEC_ID_NONE && vcodec_ctx_->pix_fmt == AV_PIX_FMT_NONE) {
        onarrived = [this](const av::frame& frame, auto) {
            vfmt.pix_fmt = static_cast<AVPixelFormat>(frame->format);
        };

        start();
        for (int i = 0; i < 100 && vfmt.pix_fmt == AV_PIX_FMT_NONE; ++i) {
            std::this_thread::sleep_for(25ms);
        }
        stop();
        onarrived = [](auto, auto) {};

        if (vfmt.pix_fmt == AV_PIX_FMT_NONE) {
            ready_ = false;
            LOG(ERROR) << "[       V4L2] failed to probe the pixel format";
            return -1;
        }
    }

    DLOG(INFO) << fmt::format("[       V4L2] {}, {}", avcodec_get_name(codec_id), av::to_string(vfmt));

    return 0;
}

int V4l2Capturer::v4l2_start_capture()
{
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

    return 0;
}

int V4l2Capturer::start()
{
    if (!ready_ || running_) {
        LOG(ERROR) << "[       V4L2] not ready or already running";
        return -1;
    }

    if (v4l2_start_capture() < 0) return -1;

    running_ = true;
    thread_  = std::jthread([this] {
        probe::thread::set_name("V4L2-" + name_);

        fd_set      fds{};
        timeval     tv{};
        v4l2_buffer buf{};
        av::packet  pkt{};

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

            // dequeue
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (::v4l2_ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
                if (errno == EAGAIN) {
                    continue;
                }
                LOG(ERROR) << name_ << ": failed to dequeue buffer";
                running_ = false;
                continue;
            }
            defer(::v4l2_ioctl(fd_, VIDIOC_QBUF, &buf));

            //
            if (av_new_packet(pkt.put(), static_cast<int>(buf.bytesused)) < 0) {
                LOG(ERROR) << name_ << ": failed to allocate packet";
                running_ = false;
                continue;
            }

            std::memcpy(pkt->data, buf_mappings_[buf.index].data, buf.bytesused);
            pkt->pts = av::clock::ns().count();

            video_decode(pkt);
        }
    });
    return 0;
}

int V4l2Capturer::video_decode(const av::packet& pkt)
{
    auto ret = avcodec_send_packet(vcodec_ctx_, pkt.get());
    while (ret >= 0) {
        ret = avcodec_receive_frame(vcodec_ctx_, frame_.put());
        if (ret == AVERROR(EAGAIN)) {
            break;
        }
        else if (ret < 0) {
            running_ = false;
            LOG(ERROR) << "[V] DECODING ERROR, ABORTING";
            break;
        }

        onarrived(frame_, AVMEDIA_TYPE_VIDEO);
    }

    return ret;
}

bool V4l2Capturer::has(const AVMediaType type) const { return type == AVMEDIA_TYPE_VIDEO; }

void V4l2Capturer::stop()
{
    running_ = false;

    constexpr v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::v4l2_ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) {
        LOG(ERROR) << "[       V4L2] failed to stop stream";
    }

    if (thread_.joinable()) thread_.join();
}

V4l2Capturer::~V4l2Capturer()
{
    ready_ = false;

    stop();

    for (const auto& [size, data] : buf_mappings_) {
        if (data != MAP_FAILED && data != nullptr) {
            ::v4l2_munmap(data, size);
        }
    }

    avcodec_free_context(&vcodec_ctx_);

    if (fd_ != -1) ::v4l2_close(fd_);
}

#endif