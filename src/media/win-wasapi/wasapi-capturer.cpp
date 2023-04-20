#ifdef _WIN32

#include "win-wasapi.h"
#include "wasapi-capturer.h"
#include "fmt/format.h"
#include "utils.h"
#include "defer.h"
#include "logging.h"

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC        10000000

#define RETURN_ON_ERROR(hres)  if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

void WasapiCapturer::init_format(WAVEFORMATEX* wfex)
{
    DWORD layout = 0;
    if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto ext = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(wfex);
        layout = ext->dwChannelMask;
    }

    afmt = aformat_t{
        static_cast<int>(wfex->nSamplesPerSec),
        wfex->nChannels,
        AV_SAMPLE_FMT_FLT,
        wasapi::to_ffmpeg_channel_layout(layout, wfex->nChannels),
        { 1, OS_TIME_BASE }
    };

    LOG(INFO) << fmt::format("[    WASAPI] [{}] sample_rate = {}, sample_fmt = {}, channels = {}, channel_layout = {}, tbn = {}/{}",
        device_.name,
        afmt.sample_rate, av_get_sample_fmt_name(afmt.sample_fmt), afmt.channels, channel_layout_name(afmt.channels, afmt.channel_layout),
        afmt.time_base.num, afmt.time_base.den);
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
int WasapiCapturer::open(avdevice_t::io_t dt)
{
    type_ = dt;

    // TODO: conflict with Qt
    //CHECK(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));

    IMMDeviceEnumerator* enumerator = nullptr;
    RETURN_ON_ERROR(CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    ));
    defer(SAFE_RELEASE(enumerator));

    IMMDevice* device = nullptr;
    RETURN_ON_ERROR(enumerator->GetDefaultAudioEndpoint(
        (type_ == avdevice_t::SOURCE) ? eCapture : eRender,
        (type_ == avdevice_t::SOURCE) ? eCommunications : eConsole,
        &device
    ));
    defer(SAFE_RELEASE(device));

    device_ = wasapi::device_info(device).value_or(avdevice_t{});

    if (init_capturer(device) != 0) {
        return -1;
    }

    if (type_ == avdevice_t::SINK) {
        if (init_silent_render(device) != 0) {
            return -1;
        }
    }

    if (!(frame_ = av_frame_alloc())) {
        LOG(ERROR) << "failed to alloc an audio frame.";
        return -1;
    }

    ready_ = true;
    return 0;
}

int WasapiCapturer::init_capturer(IMMDevice* device)
{
    RETURN_ON_ERROR(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&capturer_audio_client_));

    WAVEFORMATEX* wfex = nullptr;
    RETURN_ON_ERROR(capturer_audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    init_format(wfex);

    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (type_ == avdevice_t::SINK)
        flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

    RETURN_ON_ERROR(capturer_audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED, flags, REFTIMES_PER_SEC, 0, wfex, nullptr));

    RETURN_ON_ERROR(capturer_audio_client_->GetService(IID_PPV_ARGS(&capturer_)));

    // events
    if (!(STOP_EVENT = CreateEvent(nullptr, true, false, nullptr))) {
        LOG(ERROR) << "failed to create STOP_EVENT.";
        return -1;
    }

    if (!(AUDIO_SAMPLES_READY_EVENT = CreateEvent(nullptr, false, false, nullptr))) {
        LOG(ERROR) << "failed to create AUDIO_SAMPLES_READY_EVENT.";
        return -1;
    }

    RETURN_ON_ERROR(capturer_audio_client_->SetEventHandle(AUDIO_SAMPLES_READY_EVENT));

    return 0;
}

int WasapiCapturer::init_silent_render(IMMDevice* device)
{
    RETURN_ON_ERROR(device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&render_audio_client_));

    WAVEFORMATEX* wfex = nullptr;
    RETURN_ON_ERROR(render_audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    RETURN_ON_ERROR(render_audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED, 0, REFTIMES_PER_SEC, 0, wfex, nullptr));

    UINT32 buffer_size = 0;
    RETURN_ON_ERROR(render_audio_client_->GetBufferSize(&buffer_size));

    RETURN_ON_ERROR(render_audio_client_->GetService(IID_PPV_ARGS(&render_)));

    LPBYTE buffer = nullptr;
    RETURN_ON_ERROR(render_->GetBuffer(buffer_size, &buffer));

    memset(buffer, 0, buffer_size * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT));

    RETURN_ON_ERROR(render_->ReleaseBuffer(buffer_size, 0));
    
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
    eof_ = 0x00;

    start_time_ = os_gettime_ns();
    thread_ = std::thread([this]() {
        platform::util::thread_set_name("wasapi-" + avdevice_t::io_type_name(type_));
        run_f(); 
    });

    return 0;
}

int WasapiCapturer::run_f()
{
    LOG(INFO) << "[A] STARTED";

    UINT32 nb_samples;
    BYTE* data_ptr;
    UINT32 packet_size = 0;
    DWORD flags;
    UINT64 ts = 0;

    const HANDLE events[] = { STOP_EVENT, AUDIO_SAMPLES_READY_EVENT };

    buffer_.clear();
    while (running_) {
        switch (WaitForMultipleObjects(2, events, false, INFINITE))
        {
        case WAIT_OBJECT_0 + 0: // STOP_EVENT
            running_ = false;
            break;

        case WAIT_OBJECT_0 + 1: // AUDIO_SAMPLES_READY_EVENT
            LOG_IF(WARNING, buffer_.full()) << "[A] buffer is full, drop a packet";

            while (true)
            {
                if (FAILED(capturer_->GetNextPacketSize(&packet_size))) {
                    running_ = false;
                    LOG(ERROR) << "[A] failed to get the next packet size, exit";
                    return -1;
                }

                if (!packet_size)
                    break;

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

        default:
            LOG(ERROR) << "";
            break;
        }
    } // !while(running_)

    eof_ = 0x01;

    RETURN_ON_ERROR(capturer_audio_client_->Stop());

    LOG(INFO) << "[A] EXITED";
    return 0;
}

int WasapiCapturer::process_received_data(BYTE * data_ptr, UINT32 nb_samples, UINT64 ts)
{
    frame_->pts = os_gettime_ns() - av_rescale(nb_samples, OS_TIME_BASE, afmt.sample_rate);
    frame_->pkt_dts = frame_->pts;

    frame_->nb_samples = nb_samples;
    frame_->format = afmt.sample_fmt;
    frame_->sample_rate = afmt.sample_rate;
    frame_->channels = afmt.channels;
    frame_->channel_layout = afmt.channel_layout;

    av_frame_get_buffer(frame_, 0);
    if (av_samples_copy(
        (uint8_t**)frame_->data, (uint8_t* const*)&data_ptr,
        0, 0,
        nb_samples, afmt.channels, afmt.sample_fmt) < 0) {
        LOG(ERROR) << "av_samples_copy";
        return -1;
    }

    if (muted_) {
        av_samples_set_silence(
            frame_->data, 0,
            frame_->nb_samples, frame_->channels,
            static_cast<AVSampleFormat>(frame_->format)
        );
    }

    DLOG(INFO) << fmt::format("[A]  frame = {:>5d}, pts = {:>14d}, samples = {:>6d}, muted = {}",
        frame_number_++, frame_->pts, frame_->nb_samples, muted_);

    buffer_.push([this](AVFrame* pushed) {
        av_frame_unref(pushed);
        av_frame_move_ref(pushed, frame_);
    });

    return 0;
}

std::string WasapiCapturer::format_str(int type) const
{
    switch (type)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        return fmt::format(
            "sample_rate={}:sample_fmt={}:channels={}:channel_layout={}:time_base={}/{}",
            afmt.sample_rate, afmt.sample_fmt,
            afmt.channels, afmt.channel_layout,
            afmt.time_base.num, afmt.time_base.den
        );
    }
    default: 
        LOG(ERROR) << "unsupported media type : " << av_get_media_type_string(static_cast<AVMediaType>(type)); 
        return {};
    }
}

AVRational WasapiCapturer::time_base(int type) const
{
    switch (type)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        return afmt.time_base;
    }
    default:
        LOG(ERROR) << "[    WASAPI] unsupported media type : " << av_get_media_type_string(static_cast<AVMediaType>(type));
        return OS_TIME_BASE_Q;
    }
}

int WasapiCapturer::produce(AVFrame* frame, int type)
{
    switch (type)
    {
    case AVMEDIA_TYPE_AUDIO:
        if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

        buffer_.pop([frame](AVFrame* popped) {
            av_frame_unref(frame);
            av_frame_move_ref(frame, popped);
        });
        return 0;

    default:
        LOG(ERROR) << "[    WASAPI] unsupported media type : " << av_get_media_type_string(static_cast<AVMediaType>(type));
        return -1;
    }
}

int WasapiCapturer::destroy()
{
    ::SetEvent(STOP_EVENT);

    running_ = false;
    ready_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }

    buffer_.clear();
    start_time_ = AV_NOPTS_VALUE;

    SAFE_RELEASE(capturer_audio_client_);
    SAFE_RELEASE(capturer_);

    if (render_audio_client_) {
        render_audio_client_->Stop();
    }
    SAFE_RELEASE(render_audio_client_);
    SAFE_RELEASE(render_);

    if (STOP_EVENT && STOP_EVENT != INVALID_HANDLE_VALUE) {
        ::CloseHandle(STOP_EVENT);
    }

    // TODO : Exception thrown at 0x00007FFC1FC72D6A (ntdll.dll) in capturer.exe: 0xC0000008: An invalid handle was specified.
    if (AUDIO_SAMPLES_READY_EVENT && AUDIO_SAMPLES_READY_EVENT != INVALID_HANDLE_VALUE) {
        ::CloseHandle(AUDIO_SAMPLES_READY_EVENT);
    }

    if (frame_) {
        av_frame_free(&frame_);
    }
    frame_number_ = 0;

    //CoUninitialize();

    LOG(INFO) << "[    WASAPI] RESET";

    return 0;
}

#endif // _WIN32
