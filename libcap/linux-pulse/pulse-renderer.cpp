#include "libcap/linux-pulse/pulse-renderer.h"

#ifdef __linux__

#include "libcap/linux-pulse/linux-pulse.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <probe/defer.h>

PulseAudioRenderer::PulseAudioRenderer() { pulse::init(); }

int PulseAudioRenderer::open(const std::string&, RenderFlags)
{
    constexpr pa_sample_spec spec{
        .format   = PA_SAMPLE_S16LE,
        .rate     = 48000,
        .channels = 2,
    };

    format_ = {
        .sample_rate    = 48000,
        .sample_fmt     = AV_SAMPLE_FMT_S16,
        .channels       = 2,
        .channel_layout = AV_CH_LAYOUT_STEREO,
        .time_base      = { 1, 48000 },
    };

    if (!::pa_sample_spec_valid(&spec)) {
        LOG(ERROR) << "[PULSE-AUDIO] invalid pulse audio format";
        return -1;
    }

    stream_ = pulse::stream::create("PLAYER-AUDIO-RENDER", &spec, nullptr);
    if (!stream_) {
        LOG(ERROR) << "[PULSE-AUDIO] can not create playback stream.";
        return -1;
    }

    bytes_per_frame_ = ::pa_frame_size(&spec);

    pulse::loop_lock();
    defer(pulse::loop_unlock());

    ::pa_stream_set_state_callback(stream_, pulse_stream_state_callback, this);
    ::pa_stream_set_latency_update_callback(stream_, pulse_stream_latency_callback, this);

    buffer_attrs_ = {
        .maxlength = 1024 * bytes_per_frame_ * 2,
        .tlength   = 1024 * bytes_per_frame_,
        .prebuf    = 0,
        .minreq    = static_cast<uint32_t>(-1),
        .fragsize  = 0,
    };
    if (::pa_stream_connect_playback(stream_, nullptr, &buffer_attrs_,
                                     PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING |
                                         PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY,
                                     nullptr, nullptr) != 0) {
        LOG(ERROR) << "[PULSE-AUDIO] failed to connect playback.";
        return -1;
    }

    while (!stream_ready_)
        pulse::wait();

    buffer_attrs_ = *pa_stream_get_buffer_attr(stream_);

    ready_ = true;

    LOG(INFO) << "[PULSE-AUDIO] opened";

    return 0;
}

void PulseAudioRenderer::pulse_stream_success_callback(pa_stream *, int success, void *userdata)
{
    const auto self      = static_cast<PulseAudioRenderer *>(userdata);
    self->stream_retval_ = success;
    pulse::signal(0);
}

void PulseAudioRenderer::pulse_stream_write_callback(pa_stream *stream, size_t bytes, void *userdata)
{
    const auto self = static_cast<PulseAudioRenderer *>(userdata);

    void *buffer = nullptr;
    ::pa_stream_begin_write(stream, &buffer, &bytes);
    memset(buffer, 0, bytes);

    // fixme: assume that we always have enough frames
    self->callback(reinterpret_cast<uint8_t **>(&buffer), bytes / self->bytes_per_frame_, av::clock::ns());
    ::pa_stream_write(stream, buffer, bytes, nullptr, 0, PA_SEEK_RELATIVE);
}

void PulseAudioRenderer::pulse_stream_latency_callback(pa_stream *, void *)
{
    // DLOG(INFO) << "[PULSE-AUDIO] latency updated";
    pulse::signal(0);
}

void PulseAudioRenderer::pulse_stream_state_callback(pa_stream *stream, void *userdata)
{
    const auto self = static_cast<PulseAudioRenderer *>(userdata);

    switch (::pa_stream_get_state(stream)) {
    case PA_STREAM_UNCONNECTED:
    case PA_STREAM_TERMINATED:
    case PA_STREAM_CREATING:    break;

    case PA_STREAM_READY:
        self->stream_ready_ = true;
        pulse::signal(0);

        LOG(INFO) << "[PULSE-AUDIO] playback stream ready";
        break;

    case PA_STREAM_FAILED: LOG(ERROR) << "[PULSE-AUDIO] playback stream error"; break;

    default:               break;
    }
}

void PulseAudioRenderer::pulse_stream_update_timing_callback(pa_stream *, int, void *) { pulse::signal(0); }

void PulseAudioRenderer::pulse_stream_underflow_callback(pa_stream *, void *) { pulse::signal(0); }

void PulseAudioRenderer::pulse_stream_drain_callback(pa_stream *, int, void *) { pulse::signal(0); }

int PulseAudioRenderer::start()
{
    pulse::stream::cork(stream_, false, pulse_stream_success_callback, this);

    pulse::loop_lock();
    ::pa_stream_set_write_callback(stream_, pulse_stream_write_callback, this);
    ::pa_stream_set_underflow_callback(stream_, pulse_stream_underflow_callback, this);
    pulse::loop_unlock();

    LOG(INFO) << "[PULSE-AUDIO] STARTED";
    return 0;
}

int PulseAudioRenderer::reset() { return 0; }

int PulseAudioRenderer::stop()
{
    stream_ready_ = false;

    if (stream_) {
        pulse::loop_lock();

        ::pa_stream_set_write_callback(stream_, nullptr, nullptr);
        ::pa_stream_set_state_callback(stream_, nullptr, nullptr);
        ::pa_stream_set_underflow_callback(stream_, nullptr, nullptr);

        ::pa_stream_disconnect(stream_);
        ::pa_stream_unref(stream_);
        stream_ = nullptr;

        pulse::loop_unlock();
    }

    LOG(INFO) << "[PULSE-AUDIO] STOPPED";

    return 0;
}

PulseAudioRenderer::~PulseAudioRenderer()
{
    stop();

    pulse::unref();

    LOG(INFO) << "[PULSE-AUDIO] ~";
}

int PulseAudioRenderer::mute(bool muted)
{
    if (!stream_) return av::err_t::nullpointer;

    return pulse::stream::set_sink_mute(stream_, muted);
}

bool PulseAudioRenderer::muted() const { return stream_ && pulse::stream::get_sink_muted(stream_); }

// 0.0 ~ 1.0
int PulseAudioRenderer::set_volume(const float value)
{
    if (!stream_) return av::err_t::nullpointer;

    pa_cvolume volume{};
    ::pa_cvolume_set(&volume, format_.channels, pa_sw_volume_from_linear(value));
    return pulse::stream::set_sink_volume(stream_, &volume);
}

float PulseAudioRenderer::volume() const
{
    if (!stream_) return av::err_t::nullpointer;

    return pulse::stream::get_sink_volume(stream_);
}

int PulseAudioRenderer::pause()
{
    if (!stream_) return av::err_t::nullpointer;

    return !paused() ? pulse::stream::cork(stream_, true, pulse_stream_success_callback, this) : 0;
}

int PulseAudioRenderer::resume()
{
    if (!stream_) return av::err_t::nullpointer;

    return paused() ? pulse::stream::cork(stream_, false, pulse_stream_success_callback, this) : 0;
}

bool PulseAudioRenderer::paused() const { return stream_ && ::pa_stream_is_corked(stream_); }

#endif
