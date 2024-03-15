#ifdef _WIN32

#include "libcap/win-wasapi/wasapi-capturer.h"

#include "libcap/win-wasapi/win-wasapi.h"
#include "logging.h"

#include <probe/defer.h>
#include <probe/util.h>
#include <winrt/base.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC 10000000

void WasapiCapturer::InitWaveFormat(WAVEFORMATEX *wfex)
{
    DWORD layout = 0;
    if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto ext = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(wfex);
        layout         = ext->dwChannelMask;
    }

    afmt = av::aformat_t{
        .sample_rate    = static_cast<int>(wfex->nSamplesPerSec),
        .sample_fmt     = AV_SAMPLE_FMT_FLT,
        .channels       = wfex->nChannels,
        .channel_layout = wasapi::to_ffmpeg_channel_layout(layout, wfex->nChannels),
        .time_base      = { 1, OS_TIME_BASE },
    };

    logi("[  WASAPI-C] [{:>10}] {}", device_.name, av::to_string(afmt));
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
int WasapiCapturer::open(const std::string& name, std::map<std::string, std::string> options)
{
    try {
        winrt::com_ptr<IMMDeviceEnumerator> enumerator{};
        winrt::check_hresult(::CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr, CLSCTX_ALL,
                                                winrt::guid_of<IMMDeviceEnumerator>(),
                                                enumerator.put_void()));

        winrt::com_ptr<IMMDevice> device{};
        winrt::check_hresult(enumerator->GetDevice(probe::util::to_utf16(name).c_str(), device.put()));

        device_ = wasapi::device_info(device.get()).value_or(av::device_t{});

        InitCapturer(device.get());

        if (!!(device_.type & av::device_type_t::sink)) InitSlientRenderer(device.get());
    }
    catch (const winrt::hresult_error& e) {
        loge("{}", probe::util::to_utf8(e.message().c_str()));
        return -1;
    }

    ready_ = true;
    return 0;
}

void WasapiCapturer::InitCapturer(IMMDevice *device)
{
    winrt::check_hresult(device->Activate(winrt::guid_of<IAudioClient>(), CLSCTX_ALL, nullptr,
                                          capturer_audio_client_.put_void()));

    WAVEFORMATEX *wfex = nullptr;
    winrt::check_hresult(capturer_audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    InitWaveFormat(wfex);

    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (av::is_sink(device_)) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

    winrt::check_hresult(capturer_audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
                                                            REFTIMES_PER_SEC, 0, wfex, nullptr));

    winrt::check_hresult(
        capturer_audio_client_->GetService(winrt::guid_of<IAudioCaptureClient>(), capturer_.put_void()));

    // events
    STOP_EVENT.attach(CreateEvent(nullptr, true, false, L"Stop"));
    ARRIVED_EVENT.attach(CreateEvent(nullptr, false, false, L"Arrived"));

    winrt::check_hresult(capturer_audio_client_->SetEventHandle(ARRIVED_EVENT.get()));
}

void WasapiCapturer::InitSlientRenderer(IMMDevice *device)
{
    winrt::check_hresult(device->Activate(winrt::guid_of<IAudioClient>(), CLSCTX_ALL, nullptr,
                                          render_audio_client_.put_void()));

    WAVEFORMATEX *wfex = nullptr;
    winrt::check_hresult(render_audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    winrt::check_hresult(
        render_audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, REFTIMES_PER_SEC, 0, wfex, nullptr));

    UINT32 buffer_size = 0;
    winrt::check_hresult(render_audio_client_->GetBufferSize(&buffer_size));

    winrt::check_hresult(
        render_audio_client_->GetService(winrt::guid_of<IAudioRenderClient>(), render_.put_void()));

    LPBYTE buffer = nullptr;
    winrt::check_hresult(render_->GetBuffer(buffer_size, &buffer));

    std::memset(buffer, 0, buffer_size * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT));

    winrt::check_hresult(render_->ReleaseBuffer(buffer_size, 0));

    winrt::check_hresult(render_audio_client_->Start());
}

int WasapiCapturer::start()
{
    if (!ready_ || running_) {
        logw("[  WASAPI-C] not ready or running.");
        return -1;
    }

    if (FAILED(capturer_audio_client_->Start())) {
        loge("[  WASAPI-C] failed to start audio client.");
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
                        loge("[A] failed to get the next packet size, exit");
                        break;
                    }

                    if (!packet_size) break;

                    // Get the available data in the shared buffer.
                    if (FAILED(capturer_->GetBuffer(&data_ptr, &nb_samples, &flags, nullptr, &ts))) {
                        running_ = false;
                        loge("[A] failed to get the buffer, exit");
                        break;
                    }

                    ProcessReceivedData(data_ptr, nb_samples, ts);

                    if (FAILED(capturer_->ReleaseBuffer(nb_samples))) {
                        running_ = false;
                        loge("[A] failed to release buffer, exit");
                        break;
                    }
                }
                break;

            default: loge("[A] unknown event."); break;
            }
        } // !while(running_)

        eof_ = 0x01;

        capturer_audio_client_->Stop();

        logi("[A] THREAD EXITED");
    });

    return 0;
}

int WasapiCapturer::ProcessReceivedData(BYTE *data_ptr, const UINT32 nb_samples, UINT64) const
{
    const av::frame frame{};

    frame->pts         = av::clock::ns().count() - av_rescale(nb_samples, OS_TIME_BASE, afmt.sample_rate);
    frame->pkt_dts     = frame->pts;
    frame->nb_samples  = static_cast<int>(nb_samples);
    frame->format      = afmt.sample_fmt;
    frame->sample_rate = afmt.sample_rate;
    frame->channels    = afmt.channels;
    frame->channel_layout = afmt.channel_layout;

    av_frame_get_buffer(frame.get(), 0);
    if (av_samples_copy((uint8_t **)frame->data, (uint8_t *const *)&data_ptr, 0, 0, nb_samples,
                        afmt.channels, afmt.sample_fmt) < 0) {
        loge("failed to copy packet data");
        return -1;
    }

    if (muted_)
        av_samples_set_silence(frame->data, 0, frame->nb_samples, afmt.channels,
                               static_cast<AVSampleFormat>(frame->format));

    logd("[A] pts = {:>14d}, samples = {:>6d}", frame->pts, frame->nb_samples);

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

    logi("[  WASAPI-C] STOPPED");
}

WasapiCapturer::~WasapiCapturer()
{
    stop();

    logi("[  WASAPI-C] ~");
}

#endif // _WIN32
