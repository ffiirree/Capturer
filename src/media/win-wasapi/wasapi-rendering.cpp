#include "wasapi-rendering.h"
#include "utils.h"

#ifdef _WIN32

#define EXIT_ON_ERROR(hres) if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

uint64_t to_ffmpeg_channel_layout(DWORD layout, int channels)
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

int WasapiRender::open()
{
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDevice *pDevice = nullptr;
    WAVEFORMATEX *pwfx = nullptr;

    //EXIT_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    EXIT_ON_ERROR(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   (void**)&pEnumerator));
    defer(SAFE_RELEASE(pEnumerator));

    EXIT_ON_ERROR(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice));
    defer(SAFE_RELEASE(pDevice));

    EXIT_ON_ERROR(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                    nullptr, (void**)&audio_client_));

    EXIT_ON_ERROR(audio_client_->GetMixFormat(&pwfx));
    defer(CoTaskMemFree(pwfx));

    EXIT_ON_ERROR(audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                            0,
                                            hnsRequestedDuration,
                                            0,
                                            pwfx,
                                            nullptr));

    DWORD layout = 0;
    if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
        layout = pEx->dwChannelMask;
    }

    sample_rate_ = pwfx->nSamplesPerSec;
    channels_ = pwfx->nChannels;
    bit_rate_ = pwfx->nAvgBytesPerSec;
    sample_fmt_ = AV_SAMPLE_FMT_FLT;
    channel_layout_ = to_ffmpeg_channel_layout(layout, channels_);
    time_base_ = { 1, sample_rate_ };

    // Get the actual size of the allocated buffer.
    EXIT_ON_ERROR(audio_client_->GetBufferSize(&buffer_nb_frames_))

    EXIT_ON_ERROR(audio_client_->GetService(__uuidof(IAudioRenderClient),(void**)&render_client_));

    ready_ = true;
    return 0;
}

int WasapiRender::render_f()
{
    REFERENCE_TIME hnsActualDuration;
    UINT32 available_nb_frames;
    UINT32 padding_nb_frames;
    BYTE *buffer = nullptr;
    DWORD flags = 0;

    // Grab the entire buffer for the initial fill operation.
    EXIT_ON_ERROR(render_client_->GetBuffer(buffer_nb_frames_, &buffer));

    // Load the initial data into the shared buffer.
    EXIT_ON_ERROR(callback_(&buffer, buffer_nb_frames_ * av_get_bytes_per_sample(sample_fmt_), &flags))

    EXIT_ON_ERROR(render_client_->ReleaseBuffer(buffer_nb_frames_, flags));

    // Calculate the actual duration of the allocated buffer.
    hnsActualDuration = (double)REFTIMES_PER_SEC * buffer_nb_frames_ / sample_rate_;

    EXIT_ON_ERROR(audio_client_->Start());  // Start playing.

    // Each loop fills about half of the shared buffer.
    while (flags != AUDCLNT_BUFFERFLAGS_SILENT && running_)
    {
        // Sleep for half the buffer duration.
        Sleep((DWORD)(hnsActualDuration/REFTIMES_PER_MILLISEC/4));

        // See how much buffer space is available.
        EXIT_ON_ERROR(audio_client_->GetCurrentPadding(&padding_nb_frames));

        available_nb_frames = buffer_nb_frames_ - padding_nb_frames;

        // Grab all the available space in the shared buffer.
        EXIT_ON_ERROR(render_client_->GetBuffer(available_nb_frames, &buffer));

        // Get next 1/2-second of data from the audio source.
        if (callback_(&buffer, available_nb_frames * av_get_bytes_per_sample(sample_fmt_), &flags)) {
            running_ = false;
        }

        EXIT_ON_ERROR(render_client_->ReleaseBuffer(available_nb_frames, flags));
    }

    // Wait for last data in buffer to play before stopping.
    // Sleep((DWORD)(hnsActualDuration/REFTIMES_PER_MILLISEC/2));

    return audio_client_->Stop();  // Stop playing.
}

int WasapiRender::destroy()
{
    running_ = false;

    wait();

    if(audio_client_) {
        audio_client_->Stop();
        SAFE_RELEASE(audio_client_)
    }

    SAFE_RELEASE(render_client_)

    //CoUninitialize();

    return 0;
}

#endif // !_WIN32