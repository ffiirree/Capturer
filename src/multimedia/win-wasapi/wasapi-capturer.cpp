#ifdef _WIN32

#include "wasapi-capturer.h"

#include "fmt/format.h"
#include "logging.h"
#include "probe/defer.h"
#include "probe/util.h"
#include "utils.h"
#include "win-wasapi.h"

#include <iterator>
#include <winrt/base.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC 10'000'000

void WasapiCapturer::init_format(WAVEFORMATEX *wfex)
{
    DWORD layout = 0;
    if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto ext = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(wfex);
        layout   = ext->dwChannelMask;
    }

    afmt = av::aformat_t{
        .sample_rate    = static_cast<int>(wfex->nSamplesPerSec),
        .channels       = wfex->nChannels,
        .sample_fmt     = AV_SAMPLE_FMT_FLT,
        .channel_layout = wasapi::to_ffmpeg_channel_layout(layout, wfex->nChannels),
        .time_base      = { 1, OS_TIME_BASE },
    };

    LOG(INFO) << fmt::format("[    WASAPI] [{:>10}] {}", device_.name, av::to_string(afmt));
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
int WasapiCapturer::open(const std::string& name, std::map<std::string, std::string> options)
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator{};
    RETURN_NEGV_IF_FAILED(::CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr, CLSCTX_ALL,
                                             winrt::guid_of<IMMDeviceEnumerator>(), enumerator.put_void()));

    winrt::com_ptr<IMMDevice> device{};
    RETURN_NEGV_IF_FAILED(enumerator->GetDevice(probe::util::to_utf16(name).c_str(), device.put()));

    device_ = wasapi::device_info(device.get()).value_or(av::device_t{});

    if (init_capturer(device.get()) != 0) {
        return -1;
    }

    if (any(device_.type & av::device_type_t::sink)) {
        if (init_silent_render(device.get()) != 0) {
            return -1;
        }
    }

    ready_ = true;
    return 0;
}

int WasapiCapturer::init_capturer(IMMDevice *device)
{
    RETURN_NEGV_IF_FAILED(device->Activate(winrt::guid_of<IAudioClient>(), CLSCTX_ALL, nullptr,
                                           capturer_audio_client_.put_void()));

    WAVEFORMATEX *wfex = nullptr;
    RETURN_NEGV_IF_FAILED(capturer_audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    init_format(wfex);

    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (av::is_sink(device_)) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

    RETURN_NEGV_IF_FAILED(capturer_audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
                                                             REFTIMES_PER_SEC, 0, wfex, nullptr));

    RETURN_NEGV_IF_FAILED(
        capturer_audio_client_->GetService(winrt::guid_of<IAudioCaptureClient>(), capturer_.put_void()));

    // events
    if (STOP_EVENT = CreateEvent(nullptr, true, false, nullptr); !STOP_EVENT) {
        LOG(ERROR) << "failed to create STOP_EVENT.";
        return -1;
    }

    if (ARRIVED_EVENT = CreateEvent(nullptr, false, false, nullptr); !ARRIVED_EVENT) {
        LOG(ERROR) << "failed to create ARRIVED_EVENT.";
        return -1;
    }

    RETURN_NEGV_IF_FAILED(capturer_audio_client_->SetEventHandle(ARRIVED_EVENT));

    return 0;
}

int WasapiCapturer::init_silent_render(IMMDevice *device)
{
    RETURN_NEGV_IF_FAILED(device->Activate(winrt::guid_of<IAudioClient>(), CLSCTX_ALL, nullptr,
                                           render_audio_client_.put_void()));

    WAVEFORMATEX *wfex = nullptr;
    RETURN_NEGV_IF_FAILED(render_audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    RETURN_NEGV_IF_FAILED(
        render_audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, REFTIMES_PER_SEC, 0, wfex, nullptr));

    UINT32 buffer_size = 0;
    RETURN_NEGV_IF_FAILED(render_audio_client_->GetBufferSize(&buffer_size));

    RETURN_NEGV_IF_FAILED(
        render_audio_client_->GetService(winrt::guid_of<IAudioRenderClient>(), render_.put_void()));

    LPBYTE buffer = nullptr;
    RETURN_NEGV_IF_FAILED(render_->GetBuffer(buffer_size, &buffer));

    memset(buffer, 0, buffer_size * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT));

    RETURN_NEGV_IF_FAILED(render_->ReleaseBuffer(buffer_size, 0));

    render_audio_client_->Start();

    return 0;
}

int WasapiCapturer::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(ERROR) << "[  WASAPI] not ready or running.";
        return -1;
    }

    if (FAILED(capturer_audio_client_->Start())) {
        LOG(ERROR) << "[  WASAPI] failed to start audio client.";
        return -1;
    }

    running_ = true;
    eof_     = 0x00;

    start_time_ = os_gettime_ns();
    thread_     = std::thread([this]() {
        probe::thread::set_name("wasapi-" + device_.id);
        run_f();
    });

    return 0;
}

int WasapiCapturer::run_f()
{
    LOG(INFO) << "[A] STARTED";

    UINT32 nb_samples  = 0;
    BYTE *data_ptr     = nullptr;
    UINT32 packet_size = 0;
    DWORD flags        = 0;
    UINT64 ts          = 0;

    const HANDLE events[] = { STOP_EVENT, ARRIVED_EVENT };

    buffer_.clear();
    while (running_) {
        switch (::WaitForMultipleObjects(static_cast<DWORD>(std::size(events)), events, false, INFINITE)) {
        case WAIT_OBJECT_0 + 0: // STOP_EVENT
            running_ = false;
            break;

        case WAIT_OBJECT_0 + 1: // ARRIVED_EVENT
            LOG_IF(WARNING, buffer_.full()) << "[A] buffer is full, drop a packet";

            while (true) {
                if (FAILED(capturer_->GetNextPacketSize(&packet_size))) {
                    running_ = false;
                    LOG(ERROR) << "[A] failed to get the next packet size, exit";
                    return -1;
                }

                if (!packet_size) break;

                // Get the available data in the shared buffer.
                if (FAILED(capturer_->GetBuffer(&data_ptr, &nb_samples, &flags, nullptr, &ts))) {
                    running_ = false;
                    LOG(ERROR) << "[A] failed to get the buffer, exit";
                    return -1;
                }

                process_received_data(data_ptr, nb_samples, ts);

                if (FAILED(capturer_->ReleaseBuffer(nb_samples))) {
                    running_ = false;
                    LOG(ERROR) << "[A] failed to release buffer, exit";
                    return -1;
                }
            }
            break;

        default: LOG(ERROR) << "[A] unknown event."; break;
        }
    } // !while(running_)

    eof_ = 0x01;

    RETURN_NEGV_IF_FAILED(capturer_audio_client_->Stop());

    LOG(INFO) << "[A] EXITED";
    return 0;
}

int WasapiCapturer::process_received_data(BYTE *data_ptr, UINT32 nb_samples, UINT64)
{
    frame_.unref();

    frame_->pts            = os_gettime_ns() - av_rescale(nb_samples, OS_TIME_BASE, afmt.sample_rate);
    frame_->pkt_dts        = frame_->pts;
    frame_->nb_samples     = nb_samples;
    frame_->format         = afmt.sample_fmt;
    frame_->sample_rate    = afmt.sample_rate;
    frame_->channels       = afmt.channels;
    frame_->channel_layout = afmt.channel_layout;

    av_frame_get_buffer(frame_.get(), 0);
    if (av_samples_copy((uint8_t **)frame_->data, (uint8_t *const *)&data_ptr, 0, 0, nb_samples,
                        afmt.channels, afmt.sample_fmt) < 0) {
        LOG(ERROR) << "av_samples_copy";
        return -1;
    }

    if (muted_) {
        av_samples_set_silence(frame_->data, 0, frame_->nb_samples, frame_->channels,
                               static_cast<AVSampleFormat>(frame_->format));
    }

    DLOG(INFO) << fmt::format("[A]  frame = {:>5d}, pts = {:>14d}, samples = {:>6d}, muted = {}",
                              frame_number_++, frame_->pts, frame_->nb_samples, muted_.load());

    buffer_.push([this](AVFrame *pushed) {
        av_frame_unref(pushed);
        av_frame_move_ref(pushed, frame_.get());
    });

    return 0;
}

std::string WasapiCapturer::format_str(int type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: return av::to_string(afmt);
    default:
        LOG(ERROR) << "unsupported media type : " << av::to_string(static_cast<AVMediaType>(type));
        return {};
    }
}

AVRational WasapiCapturer::time_base(int type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: {
        return afmt.time_base;
    }
    default:
        LOG(ERROR) << "[    WASAPI] unsupported media type : "
                   << av::to_string(static_cast<AVMediaType>(type));
        return OS_TIME_BASE_Q;
    }
}

int WasapiCapturer::produce(AVFrame *frame, int type)
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

        buffer_.pop([frame](AVFrame *popped) {
            av_frame_unref(frame);
            av_frame_move_ref(frame, popped);
        });
        return 0;

    default:
        LOG(ERROR) << "[    WASAPI] unsupported media type : "
                   << av::to_string(static_cast<AVMediaType>(type));
        return -1;
    }
}

int WasapiCapturer::destroy()
{
    ::SetEvent(STOP_EVENT);

    running_ = false;
    ready_   = false;

    if (thread_.joinable()) {
        thread_.join();
    }

    buffer_.clear();
    start_time_ = AV_NOPTS_VALUE;

    if (render_audio_client_) {
        render_audio_client_->Stop();
    }

    if (STOP_EVENT && STOP_EVENT != INVALID_HANDLE_VALUE) {
        ::CloseHandle(STOP_EVENT);
    }

    // TODO : Exception thrown at 0x00007FFC1FC72D6A (ntdll.dll) in capturer.exe: 0xC0000008: An invalid
    // handle was specified.
    if (ARRIVED_EVENT && ARRIVED_EVENT != INVALID_HANDLE_VALUE) {
        ::CloseHandle(ARRIVED_EVENT);
    }

    frame_number_ = 0;

    LOG(INFO) << "[    WASAPI] RESET";

    return 0;
}

#endif // _WIN32
