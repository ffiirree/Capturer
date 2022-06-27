#ifdef _WIN32

#include "wasapi-capturer.h"
#include "fmt/format.h"
#include "utils.h"
#include "logging.h"

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define RETURN_ON_ERROR(hres)  if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

uint64_t WasapiCapturer::to_ffmpeg_channel_layout(DWORD layout, int channels)
{
    switch (layout) {
    case KSAUDIO_SPEAKER_MONO: return AV_CH_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO: return AV_CH_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD: return AV_CH_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_2POINT1: return AV_CH_LAYOUT_SURROUND;
    case KSAUDIO_SPEAKER_SURROUND: return AV_CH_LAYOUT_4POINT0;
    default: return av_get_default_channel_layout(channels);
    }
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
int WasapiCapturer::open(DeviceType dt)
{
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    WAVEFORMATEX* pwfx = nullptr;

    RETURN_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    RETURN_ON_ERROR(CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    ));
    defer(SAFE_RELEASE(pEnumerator));

    RETURN_ON_ERROR(pEnumerator->GetDefaultAudioEndpoint((dt == DeviceType::DEVICE_MICROPHONE) ? eCapture : eRender, eConsole, &pDevice));
    defer(SAFE_RELEASE(pDevice));

    RETURN_ON_ERROR(pDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&audio_client_)
    );

    RETURN_ON_ERROR(audio_client_->GetMixFormat(&pwfx));
    defer(CoTaskMemFree(pwfx));

    RETURN_ON_ERROR(audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        (dt == DeviceType::DEVICE_MICROPHONE) ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK,
        hnsRequestedDuration,
        0,
        pwfx,
        nullptr
    ));

    DWORD layout = 0;
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
        //        if(IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
        //            LOG(INFO) << "here";
        //            pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        //            pEx->Samples.wValidBitsPerSample = 16;
        //            pwfx->wBitsPerSample = 16;
        //            pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
        //            pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
        //        }
        layout = pEx->dwChannelMask;
    }

    sample_rate_ = pwfx->nSamplesPerSec;
    channels_ = pwfx->nChannels;
    bit_rate_ = pwfx->nAvgBytesPerSec;
    sample_fmt_ = AV_SAMPLE_FMT_FLT;
    channel_layout_ = to_ffmpeg_channel_layout(layout, channels_);

    LOG(INFO) << fmt::format("sample_rate = {}, channels = {}, sample_fmt = {}, channel_layout = {}",
        sample_rate_, channels_, av_get_sample_fmt_name(sample_fmt_), channel_layout_);

    // Get the size of the allocated buffer.
    RETURN_ON_ERROR(audio_client_->GetBufferSize(&buffer_nb_frames_));

    RETURN_ON_ERROR(audio_client_->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&capture_client_
    ));

    ready_ = true;
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
            sample_rate_, sample_fmt_,
            channels_, channel_layout_,
            time_base_.num, time_base_.den
        );
    }
    default: return {};
    }
}

int WasapiCapturer::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        return -1;
    }

    RETURN_ON_ERROR(audio_client_->Start());

    start_time_ = av_gettime_relative();
    running_ = true;

    thread_ = std::thread([this]() { run_f(); });

    return 0;
}

int WasapiCapturer::run_f()
{
    UINT32 nb_samples;
    BYTE* data_ptr;
    UINT32 packetLength = 0;
    DWORD flags;

    buffer_.clear();
    REFERENCE_TIME hnsActualDuration = (double)REFTIMES_PER_SEC * buffer_nb_frames_ / sample_rate_;

    AVFrame* frame = av_frame_alloc();
    while (running_) {
        // Sleep for half the buffer duration.
//        Sleep(hnsActualDuration / REFTIMES_PER_MILLISEC / 2);

        RETURN_ON_ERROR(capture_client_->GetNextPacketSize(&packetLength));

        while (packetLength != 0)
        {
            // Get the available data in the shared buffer.
            if (FAILED(capture_client_->GetBuffer(
                &data_ptr,
                &nb_samples,
                &flags,
                nullptr,
                nullptr
            ))) {
                running_ = false;
                LOG(INFO) << "GetBuffer failed";
                return -1;
            }

            // Copy the available capture data to the audio sink.
            frame->pts = av_gettime_relative();
            frame->nb_samples = nb_samples;
            frame->format = sample_fmt_;
            frame->sample_rate = sample_rate_;
            frame->channels = channels_;
            frame->channel_layout = channel_layout_;


            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Allocated data will be initialized to silence.
                av_samples_alloc(frame->data, frame->linesize,
                    channels_, nb_samples, sample_fmt_, 1);
                LOG(ERROR) << "AUDCLNT_BUFFERFLAGS_SILENT";
            }
            else {
                av_frame_get_buffer(frame, 1);
                if (av_samples_copy((uint8_t**)frame->data, (uint8_t* const*)&data_ptr,
                    0, 0, nb_samples,
                    channels_, sample_fmt_) < 0) {
                    LOG(ERROR) << "av_samples_copy";
                    return -1;
                }
            }

            while (buffer_.full() && running_) {
                av_usleep(10'000); // 10ms
            }

            buffer_.push([frame](AVFrame* pushed) {
                av_frame_unref(pushed);
                av_frame_move_ref(pushed, frame);
                });

            RETURN_ON_ERROR(capture_client_->ReleaseBuffer(nb_samples));
            RETURN_ON_ERROR(capture_client_->GetNextPacketSize(&packetLength));
        }
    }

    RETURN_ON_ERROR(audio_client_->Stop());

    av_frame_free(&frame);
    return 0;
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

    default: return -1;

    }
}

int WasapiCapturer::destroy()
{
    running_ = false;
    ready_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }

    buffer_.clear();
    start_time_ = AV_NOPTS_VALUE;

    SAFE_RELEASE(audio_client_);
    SAFE_RELEASE(capture_client_);

    CoUninitialize();

    return 0;
}

#endif // _WIN32
