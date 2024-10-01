#ifdef __linux__

#include "libcap/linux-pulse/pulse-capturer.h"

#include "libcap/linux-pulse/linux-pulse.h"
#include "logging.h"

#include <fmt/format.h>
#include <probe/defer.h>

PulseCapturer::PulseCapturer() { pulse::init(); }

bool PulseCapturer::has(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: return ready_;
    default:                 return false;
    }
}

int PulseCapturer::open(const std::string& name, std::map<std::string, std::string>)
{
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
        loge("[PULSE-AUDIO] invalid pulse audio format");
        return -1;
    }

    // capture stream
    {
        stream_ = pulse::stream::create("PLAYER-AUDIO-CAPTURER", &spec, nullptr);
        if (!stream_) {
            loge("[PULSE-AUDIO] can not create playback stream.");
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
            loge("[PULSE-AUDIO] failed to connect record.");
            return -1;
        }
    }

    eof_     = 0x00;
    running_ = true;
    ready_   = true;

    logi("[PULSE-AUDIO] {} opened", name);

    return 0;
}

void PulseCapturer::pulse_stream_read_callback(pa_stream *stream, size_t /* == bytes*/, void *userdata)
{
    const auto self = static_cast<PulseCapturer *>(userdata);
    defer(pulse::signal(0));

    const void *frames = nullptr;
    size_t      bytes  = 0;

    if (::pa_stream_peek(stream, &frames, &bytes) < 0) {
        loge("[PULSE-AUDIO] failed to read frames");
        return;
    }

    if (!bytes) return;
    if (!frames) {
        pa_stream_drop(stream);
        loge("[PULSE-AUDIO] frames == nullptr");
        return;
    }

    const av::frame frame{};

    frame->nb_samples = static_cast<int>(bytes / self->bytes_per_frame_);
    // FIXME: calculate the exact pts
    frame->pts =
        av::clock::ns().count() - av_rescale(frame->nb_samples, self->afmt.sample_rate, OS_TIME_BASE);
    frame->pkt_dts     = frame->pts;
    frame->format      = self->afmt.sample_fmt;
    frame->sample_rate = self->afmt.sample_rate;
    frame->ch_layout   = AV_CHANNEL_LAYOUT_MASK(self->afmt.channels, self->afmt.channel_layout);

    av_frame_get_buffer(frame.get(), 0);
    if (av_samples_copy(frame->data, (uint8_t *const *)&frames, 0, 0, frame->nb_samples,
                        self->afmt.channels, self->afmt.sample_fmt) < 0) {
        loge("[PULSE-AUDIO] failed to copy frames");
        return;
    }

    if (self->muted_)
        av_samples_set_silence(frame->data, 0, frame->nb_samples, frame->ch_layout.nb_channels,
                               self->afmt.sample_fmt);

    logd("[A] pts = {:>14d}, samples = {:>6d}", frame->pts, frame->nb_samples);

    self->onarrived(frame, AVMEDIA_TYPE_AUDIO);

    pa_stream_drop(stream);
}

int PulseCapturer::start() { return 0; }

void PulseCapturer::stop()
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
}

PulseCapturer::~PulseCapturer()
{
    stop();
    pulse::unref();
}

#endif