#ifdef __linux__

#include "libcap/linux-pulse/pulse-capturer.h"

#include "libcap/linux-pulse/linux-pulse.h"

#include <fmt/format.h>
#include <probe/defer.h>

int PulseCapturer::open(const std::string& name, std::map<std::string, std::string>)
{
    pulse::init();

    const auto spec = pulse::source_format(name);
    afmt            = {
                   .sample_rate    = static_cast<int>(spec.rate),
                   .sample_fmt     = pulse::to_av_sample_format(spec.format),
                   .channels       = spec.channels,
                   .channel_layout = pulse::to_av_channel_layout(spec.channels),
                   .time_base      = { 1, OS_TIME_BASE },
    };

    if (afmt.sample_fmt == AV_SAMPLE_FMT_NONE || afmt.channel_layout == 0 ||
        !::pa_sample_spec_valid(&spec)) {
        LOG(ERROR) << "[PULSE-AUDIO] invalid pulse audio format";
        return -1;
    }

    // capture stream
    {
        stream_ = pulse::stream::create("PLAYER-AUDIO-CAPTURER", &spec, nullptr);
        if (!stream_) {
            LOG(ERROR) << "[PULSE-AUDIO] can not create playback stream.";
            return -1;
        }

        pulse::loop_lock();
        defer(pulse::loop_unlock());

        ::pa_stream_set_read_callback(stream_, pulse_stream_read_callback, this);

        bytes_per_frame_ = ::pa_frame_size(&spec);

        const pa_buffer_attr buffer_attr{
            .maxlength = static_cast<uint32_t>(-1),
            .tlength   = static_cast<uint32_t>(-1),
            .prebuf    = static_cast<uint32_t>(-1),
            .minreq    = static_cast<uint32_t>(-1),
            .fragsize  = static_cast<uint32_t>(::pa_usec_to_bytes(25000, &spec)),
        };
        if (::pa_stream_connect_record(stream_, name.c_str(), &buffer_attr, PA_STREAM_ADJUST_LATENCY) !=
            0) {
            LOG(ERROR) << "[PULSE-AUDIO] failed to connect record.";
            return -1;
        }
    }

    eof_     = 0x00;
    running_ = true;
    ready_   = true;

    LOG(INFO) << fmt::format("[PULSE-AUDIO] {} opened", name);

    return 0;
}

void PulseCapturer::pulse_stream_read_callback(pa_stream *stream, size_t /* == bytes*/, void *userdata)
{
    const auto self = static_cast<PulseCapturer *>(userdata);
    defer(pulse::signal(0));

    const void *frames = nullptr;
    size_t      bytes  = 0;

    if (::pa_stream_peek(stream, &frames, &bytes) < 0) {
        LOG(ERROR) << "[PULSE-AUDIO] failed to read frames";
        return;
    }

    if (!bytes) return;
    if (!frames) {
        pa_stream_drop(stream);
        LOG(ERROR) << "[PULSE-AUDIO] frames == nullptr";
        return;
    }

    self->frame_.unref();
    self->frame_->nb_samples = static_cast<int>(bytes / self->bytes_per_frame_);
    // FIXME: calculate the exact pts
    self->frame_->pts        = av::clock::ns().count() -
                        av_rescale(self->frame_->nb_samples, OS_TIME_BASE, self->afmt.sample_rate);
    self->frame_->pkt_dts        = self->frame_->pts;
    self->frame_->format         = self->afmt.sample_fmt;
    self->frame_->sample_rate    = self->afmt.sample_rate;
    self->frame_->channels       = self->afmt.channels;
    self->frame_->channel_layout = self->afmt.channel_layout;

    av_frame_get_buffer(self->frame_.get(), 0);
    if (av_samples_copy(self->frame_->data, (uint8_t *const *)&frames, 0, 0, self->frame_->nb_samples,
                        self->afmt.channels, self->afmt.sample_fmt) < 0) {
        LOG(ERROR) << "[PULSE-AUDIO] failed to copy frames";
        return;
    }

    if (self->muted_) {
        av_samples_set_silence(self->frame_->data, 0, self->frame_->nb_samples, self->frame_->channels,
                               static_cast<AVSampleFormat>(self->frame_->format));
    }

    DLOG(INFO) << fmt::format("[A]  frame = {:>5d}, pts = {:>14d}, samples = {:>6d}, muted = {}",
                              self->frame_number_++, self->frame_->pts, self->frame_->nb_samples,
                              self->muted_.load());

    self->buffer_.push([self](AVFrame *pushed) {
        av_frame_unref(pushed);
        av_frame_move_ref(pushed, self->frame_.get());
    });

    pa_stream_drop(stream);
}

int PulseCapturer::run() { return 0; }

int PulseCapturer::produce(AVFrame *frame, AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

        buffer_.pop([frame](AVFrame *popped) {
            av_frame_unref(frame);
            av_frame_move_ref(frame, popped);
        });
        return 0;

    default: LOG(ERROR) << "[PULSE-AUDIO] unsupported media type : " << av::to_string(type); return -1;
    }
}

std::string PulseCapturer::format_str(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: return av::to_string(afmt);
    default:                 LOG(ERROR) << "[PULSE-AUDIO] unsupported media type : " << av::to_string(type); return {};
    }
}

AVRational PulseCapturer::time_base(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: {
        return afmt.time_base;
    }
    default:
        LOG(ERROR) << "[PULSE-AUDIO] unsupported media type : " << av::to_string(type);
        return OS_TIME_BASE_Q;
    }
}

int PulseCapturer::destroy()
{
    running_ = false;
    ready_   = false;

    if (stream_) {
        pulse::loop_lock();
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
        pulse::loop_unlock();
    }

    bytes_per_frame_ = 1;
    pulse::unref();

    buffer_.clear();
    eof_          = 0x01;
    frame_number_ = 0;

    return 0;
}

#endif