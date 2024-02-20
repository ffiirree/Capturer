#ifdef _WIN32

#include "libcap/win-wasapi/wasapi-capturer.h"

#include "libcap/platform.h"
#include "libcap/win-wasapi/win-wasapi.h"
#include "logging.h"

#include <fmt/format.h>
#include <iterator>
#include <probe/defer.h>
#include <probe/util.h>
#include <winrt/base.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC 10000000

void WasapiCapturer::init_format(WAVEFORMATEX *wfex)
{
    DWORD layout = 0;
    if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto ext = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(wfex);
        layout   = ext->dwChannelMask;
    }

    afmt = av::aformat_t{
        .sample_rate    = static_cast<int>(wfex->nSamplesPerSec),
        .sample_fmt     = AV_SAMPLE_FMT_FLT,
        .channels       = wfex->nChannels,
        .channel_layout = wasapi::to_ffmpeg_channel_layout(layout, wfex->nChannels),
        .time_base      = { 1, OS_TIME_BASE },
    };

    LOG(INFO) << fmt::format("[  WASAPI-C] [{:>10}] {}", device_.name, av::to_string(afmt));
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
    if (STOP_EVENT.attach(CreateEvent(nullptr, true, false, nullptr)); !STOP_EVENT) {
        LOG(ERROR) << "failed to create STOP_EVENT.";
        return -1;
    }

    if (ARRIVED_EVENT.attach(CreateEvent(nullptr, false, false, nullptr)); !ARRIVED_EVENT) {
        LOG(ERROR) << "failed to create ARRIVED_EVENT.";
        return -1;
    }

    RETURN_NEGV_IF_FAILED(capturer_audio_client_->SetEventHandle(ARRIVED_EVENT.get()));

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

    RETURN_NEGV_IF_FAILED(render_audio_client_->Start());

    return 0;
}

int WasapiCapturer::start()
{
    if (!ready_ || running_) {
        LOG(ERROR) << "[  WASAPI-C] not ready or running.";
        return -1;
    }

    if (FAILED(capturer_audio_client_->Start())) {
        LOG(ERROR) << "[  WASAPI-C] failed to start audio client.";
        return -1;
    }

    running_ = true;
    eof_     = 0x00;

    start_time_ = av::clock::ns().count();
    thread_     = std::jthread([this] {
        probe::thread::set_name("WASAPI-C-" + device_.id);

        UINT32 nb_samples  = 0;
        BYTE  *data_ptr    = nullptr;
        UINT32 packet_size = 0;
        DWORD  flags       = 0;
        UINT64 ts          = 0;

        const HANDLE events[] = { STOP_EVENT.get(), ARRIVED_EVENT.get() };

        while (running_) {
            switch (
                ::WaitForMultipleObjects(static_cast<DWORD>(std::size(events)), events, false, INFINITE)) {
            case WAIT_OBJECT_0 + 0: // STOP_EVENT
                running_ = false;
                break;

            case WAIT_OBJECT_0 + 1: // ARRIVED_EVENT
                while (true) {
                    if (FAILED(capturer_->GetNextPacketSize(&packet_size))) {
                        running_ = false;
                        LOG(ERROR) << "[A] failed to get the next packet size, exit";
                        return;
                    }

                    if (!packet_size) break;

                    // Get the available data in the shared buffer.
                    if (FAILED(capturer_->GetBuffer(&data_ptr, &nb_samples, &flags, nullptr, &ts))) {
                        running_ = false;
                        LOG(ERROR) << "[A] failed to get the buffer, exit";
                        return;
                    }

                    process_received_data(data_ptr, nb_samples, ts);

                    if (FAILED(capturer_->ReleaseBuffer(nb_samples))) {
                        running_ = false;
                        LOG(ERROR) << "[A] failed to release buffer, exit";
                        return;
                    }
                }
                break;

            default: LOG(ERROR) << "[A] unknown event."; break;
            }
        } // !while(running_)

        eof_ = 0x01;

        winrt::check_hresult(capturer_audio_client_->Stop());

        DLOG(INFO) << "[A] THREAD EXITED";
    });

    return 0;
}

int WasapiCapturer::process_received_data(BYTE *data_ptr, UINT32 nb_samples, UINT64)
{
    const av::frame frame{};

    frame->pts         = av::clock::ns().count() - av_rescale(nb_samples, OS_TIME_BASE, afmt.sample_rate);
    frame->pkt_dts     = frame->pts;
    frame->nb_samples  = static_cast<int>(nb_samples);
    frame->format      = afmt.sample_fmt;
    frame->sample_rate = afmt.sample_rate;
#if FFMPEG_NEW_CHANNEL_LAYOUT
    frame->ch_layout = AV_CHANNEL_LAYOUT_MASK(afmt.channels, afmt.channel_layout);
#else
    frame->channels       = afmt.channels;
    frame->channel_layout = afmt.channel_layout;
#endif

    av_frame_get_buffer(frame.get(), 0);
    if (av_samples_copy((uint8_t **)frame->data, (uint8_t *const *)&data_ptr, 0, 0, nb_samples,
                        afmt.channels, afmt.sample_fmt) < 0) {
        LOG(ERROR) << "av_samples_copy";
        return -1;
    }

    if (muted_)
        av_samples_set_silence(frame->data, 0, frame->nb_samples, afmt.channels,
                               static_cast<AVSampleFormat>(frame->format));

    // DLOG(INFO) << fmt::format("[A]  frame = {:>5d}, pts = {:>14d}, samples = {:>6d}, muted = {}",
    //                           frame_number_++, frame->pts, frame->nb_samples, muted_.load());

    onarrived(frame, AVMEDIA_TYPE_AUDIO);

    return 0;
}

void WasapiCapturer::stop()
{
    ::SetEvent(STOP_EVENT.get());

    running_ = false;
    ready_   = false;

    if (thread_.joinable()) thread_.join();

    if (render_audio_client_) render_audio_client_->Stop();

    LOG(INFO) << "[  WASAPI-C] STOPPED";
}

WasapiCapturer::~WasapiCapturer()
{
    stop();

    LOG(INFO) << "[  WASAPI-C] ~";
}

#endif // _WIN32
