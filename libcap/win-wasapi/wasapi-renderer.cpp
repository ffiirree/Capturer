#ifdef _WIN32

#include "libcap/win-wasapi/wasapi-renderer.h"

#include "libcap/platform.h"
#include "libcap/win-wasapi/win-wasapi.h"
#include "logging.h"

#include <fmt/format.h>
#include <probe/defer.h>

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
int WasapiRenderer::open(const std::string&, RenderFlags flags)
{
    flags_ = flags;

    RETURN_NEGV_IF_FAILED(::CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr,
                                             CLSCTX_INPROC_SERVER, winrt::guid_of<IMMDeviceEnumerator>(),
                                             enumerator_.put_void()));

    RETURN_NEGV_IF_FAILED(enumerator_->GetDefaultAudioEndpoint(eRender, eMultimedia, endpoint_.put()));

    devinfo_ = wasapi::device_info(endpoint_.get()).value_or(av::device_t{});

    RETURN_NEGV_IF_FAILED(endpoint_->Activate(winrt::guid_of<IAudioClient>(), CLSCTX_INPROC_SERVER, nullptr,
                                              audio_client_.put_void()));

    // wave format
    RETURN_NEGV_IF_FAILED(audio_client_->GetMixFormat(&wfex_));

    InitializeWaveFormat(wfex_);

    //
    RETURN_NEGV_IF_FAILED(InitializeAudioEngine());

    if (flags_ & RENDER_ALLOW_STREAM_SWITCH) RETURN_NEGV_IF_FAILED(InitializeStreamSwitch());

    // events
    if (STOP_EVENT.attach(::CreateEvent(nullptr, true, false, nullptr)); !STOP_EVENT) {
        LOG(ERROR) << "[  WASAPI-R] failed to create STOP_EVENT.";
        return -1;
    }

    if (SWITCH_EVENT.attach(::CreateEvent(nullptr, false, false, nullptr)); !SWITCH_EVENT) {
        LOG(ERROR) << "[  WASAPI-R] failed to create SWITCH_EVENT.";
        return -1;
    }

    ready_ = true;

    return 0;
}

void WasapiRenderer::InitializeWaveFormat(WAVEFORMATEX *wfex)
{
    DWORD layout = 0;
    if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto ext = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(wfex);
        layout         = ext->dwChannelMask;
    }

    format_ = av::aformat_t{
        .sample_rate    = static_cast<int>(wfex->nSamplesPerSec),
        .sample_fmt     = AV_SAMPLE_FMT_FLT,
        .channels       = wfex->nChannels,
        .channel_layout = wasapi::to_ffmpeg_channel_layout(layout, wfex->nChannels),
        .time_base      = { 1, static_cast<int>(wfex->nSamplesPerSec) },
    };

    LOG(INFO) << fmt::format("[  WASAPI-R] [{:>10}] {}", devinfo_.name, av::to_string(format_));
}

HRESULT WasapiRenderer::InitializeAudioEngine()
{
    RETURN_IF_FAILED(audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                               300000 /*100-nanosecond*/, 0, wfex_, nullptr));

    RETURN_IF_FAILED(audio_client_->GetBufferSize(&buffer_frames_));

    // event
    if (REQUEST_EVENT.attach(::CreateEvent(nullptr, false, false, nullptr)); !REQUEST_EVENT) {
        LOG(ERROR) << "[  WASAPI-R] failed to create REQUEST_EVENT.";
        return -1;
    }

    RETURN_IF_FAILED(audio_client_->SetEventHandle(REQUEST_EVENT.get()));

    // services
    RETURN_IF_FAILED(audio_client_->GetService(winrt::guid_of<IAudioRenderClient>(), renderer_.put_void()));

    RETURN_IF_FAILED(audio_client_->GetService(winrt::guid_of<ISimpleAudioVolume>(), volume_.put_void()));

    RETURN_IF_FAILED(
        audio_client_->GetService(winrt::guid_of<IAudioSessionControl>(), session_.put_void()));

    return S_OK;
}

HRESULT WasapiRenderer::InitializeStreamSwitch()
{
    if (SWITCH_COMPLETE_EVENT.attach(::CreateEvent(nullptr, true, false, nullptr));
        !SWITCH_COMPLETE_EVENT) {
        LOG(ERROR) << "[  WASAPI-R] failed to create SWITCH_COMPLETE_EVENT.";
        return E_UNEXPECTED;
    }

    RETURN_IF_FAILED(session_->RegisterAudioSessionNotification(this));

    RETURN_IF_FAILED(enumerator_->RegisterEndpointNotificationCallback(this));

    return S_OK;
}

int WasapiRenderer::start()
{
    if (!ready_ || running_) {
        LOG(ERROR) << "[  WASAPI-R] not ready or running.";
        return -1;
    }

    running_ = true;

    // zero fill
    {
        BYTE *data_ptr = nullptr;
        RETURN_NEGV_IF_FAILED(renderer_->GetBuffer(buffer_frames_, &data_ptr));
        RETURN_NEGV_IF_FAILED(renderer_->ReleaseBuffer(buffer_frames_, AUDCLNT_BUFFERFLAGS_SILENT));
    }

    // start
    RETURN_NEGV_IF_FAILED(audio_client_->Start());
    thread_ = std::jthread([this]() {
        probe::thread::set_name("WASAPI-R-" + devinfo_.id);

        DLOG(INFO) << "[A] WASAPI RENDER THREAD STARTED";

        const HANDLE events[] = { STOP_EVENT.get(), REQUEST_EVENT.get(), SWITCH_EVENT.get() };
        while (running_) {
            switch (
                ::WaitForMultipleObjects(static_cast<DWORD>(std::size(events)), events, false, INFINITE)) {
            case WAIT_OBJECT_0 + 0: // STOP_EVENT
                running_ = false;
                break;

            case WAIT_OBJECT_0 + 1: // REQUEST_EVENT
                if (FAILED(RequestEventHandler(av::clock::ns())))
                    LOG(ERROR) << "failed to write data to the renderer buffer";
                break;

            case WAIT_OBJECT_0 + 2: // SWITCH_EVENT
                SwitchEventHandler();
                break;

            default: break;
            }
        }

        DLOG(INFO) << "[A] WASAPI RENDER THREAD EXITED";
    });

    return 0;
}

int WasapiRenderer::pause()
{
    std::lock_guard lock(mtx_);

    if (!audio_client_) return -1;

    RETURN_NEGV_IF_FAILED(audio_client_->Stop());
    return 0;
}

int WasapiRenderer::resume()
{
    std::lock_guard lock(mtx_);

    if (!audio_client_) return -1;

    RETURN_NEGV_IF_FAILED(audio_client_->Start());
    return 0;
}

bool WasapiRenderer::paused() const
{
    std::lock_guard lock(mtx_);

    if (!session_) return false;

    AudioSessionState state{};
    return FAILED(session_->GetState(&state)) || state != AudioSessionStateActive;
}

int WasapiRenderer::mute(const bool muted)
{
    std::lock_guard lock(mtx_);

    if (!volume_) return -1;

    RETURN_NEGV_IF_FAILED(volume_->SetMute(muted, nullptr));
    return 0;
}

bool WasapiRenderer::muted() const
{
    std::lock_guard lock(mtx_);

    if (!volume_) return false;

    BOOL muted = false;
    return SUCCEEDED(volume_->GetMute(&muted)) && muted;
}

int WasapiRenderer::set_volume(const float volume)
{
    std::lock_guard lock(mtx_);

    if (!volume_) return -1;

    RETURN_NEGV_IF_FAILED(volume_->SetMasterVolume(volume, nullptr));
    return 0;
}

float WasapiRenderer::volume() const
{
    std::lock_guard lock(mtx_);

    if (!volume_) return -1;

    float volume = 0.0;
    volume_->GetMasterVolume(&volume);
    return volume;
}

HRESULT WasapiRenderer::RequestEventHandler(std::chrono::nanoseconds ts)
{
    BYTE  *buffer         = nullptr;
    UINT32 padding_frames = 0;

    RETURN_IF_FAILED(audio_client_->GetCurrentPadding(&padding_frames));
    const UINT32 request_frames = buffer_frames_ - padding_frames;

    // Grab all the available space in the shared buffer.
    RETURN_IF_FAILED(renderer_->GetBuffer(request_frames, &buffer));
    const UINT32 wframes = callback(&buffer, request_frames, ts);
    RETURN_IF_FAILED(renderer_->ReleaseBuffer(wframes, wframes ? 0 : AUDCLNT_BUFFERFLAGS_SILENT));

    return S_OK;
}

HRESULT WasapiRenderer::SwitchEventHandler()
{
    if (switching_ == false) return S_FALSE;

    std::lock_guard lock(mtx_);

    // 1. stop rendering
    RETURN_IF_FAILED(audio_client_->Stop());

    // 2. release resources
    RETURN_IF_FAILED(session_->UnregisterAudioSessionNotification(this));

    session_      = nullptr;
    renderer_     = nullptr;
    volume_       = nullptr;
    audio_client_ = nullptr;
    endpoint_     = nullptr;

    // 3. wait for the default device to change
    //
    // There is a race between the session disconnect arriving and the new default device
    // arriving (if applicable).  Wait the shorter of 500 milliseconds or the arrival of the
    // new default device, then attempt to switch to the default device.  In the case of a
    // format change (i.e. the default device does not change), we artificially generate  a
    // new default device notification so the code will not needlessly wait 500ms before
    // re-opening on the new format.  (However, note below in step 6 that in this SDK
    // sample, we are unlikely to actually successfully absorb a format change, but a
    // real audio application implementing stream switching would re-format their
    // pipeline to deliver the new format).
    if (WaitForSingleObject(SWITCH_COMPLETE_EVENT.get(), 500) == WAIT_TIMEOUT) {
        LOG(ERROR) << "Stream switch timeout - aborting...";
        ::SetEvent(STOP_EVENT.get());
        return E_UNEXPECTED;
    }

    // 4. endpoint
    RETURN_IF_FAILED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, endpoint_.put()));

    // 5. re-instantiate the audio client
    RETURN_IF_FAILED(endpoint_->Activate(winrt::guid_of<IAudioClient>(), CLSCTX_INPROC_SERVER, nullptr,
                                         audio_client_.put_void()));

    // 6. retrieve the new mix format
    WAVEFORMATEX *wfex = nullptr;
    RETURN_IF_FAILED(audio_client_->GetMixFormat(&wfex));
    defer(CoTaskMemFree(wfex));

    if (std::memcmp(wfex, wfex_, sizeof(WAVEFORMATEX) + wfex->cbSize) != 0) {
        LOG(ERROR) << "WAVE FORMAT ERROR";
        return E_UNEXPECTED;
    }

    // 7. re-initialize the audio client
    RETURN_IF_FAILED(InitializeAudioEngine());

    // 8. re-register for session disconnect notification
    RETURN_IF_FAILED(
        audio_client_->GetService(winrt::guid_of<IAudioSessionControl>(), session_.put_void()));
    RETURN_IF_FAILED(session_->RegisterAudioSessionNotification(this));

    // Reset the stream switch complete event because it's a manual reset event.
    ::ResetEvent(SWITCH_COMPLETE_EVENT.get());

    // start rendering again
    RETURN_IF_FAILED(audio_client_->Start());

    //
    switching_ = false;

    return S_OK;
}

// https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/WASAPIRendering/WASAPIRendering.cpp
HRESULT WasapiRenderer::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR)
{
    if (flow == eRender && role == eMultimedia) {
        if (!switching_) {
            switching_ = true;
            ::SetEvent(SWITCH_EVENT.get());
        }
        ::SetEvent(SWITCH_COMPLETE_EVENT.get());
    }
    return S_OK;
}

HRESULT WasapiRenderer::OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason)
{
    switch (DisconnectReason) {
    case DisconnectReasonDeviceRemoval:
        switching_ = true;
        ::SetEvent(SWITCH_EVENT.get());
        break;

    case DisconnectReasonFormatChanged:
        switching_ = true;
        ::SetEvent(SWITCH_EVENT.get());
        ::SetEvent(SWITCH_COMPLETE_EVENT.get());
        break;
    default: break;
    }

    return S_OK;
}

int WasapiRenderer::reset()
{
    std::lock_guard lock(mtx_);

    if (!audio_client_) return -1;

    RETURN_NEGV_IF_FAILED(audio_client_->Reset());
    return 0;
}

int WasapiRenderer::stop()
{
    ::SetEvent(STOP_EVENT.get());

    running_ = false;
    ready_   = false;

    if (thread_.joinable()) thread_.join();

    LOG(INFO) << "[A] [  RENDERER] STOPPED";

    return 0;
}

WasapiRenderer::~WasapiRenderer()
{
    stop();

    if (wfex_) CoTaskMemFree(wfex_);

    InterlockedDecrement(&refs);

    LOG(INFO) << "[A] [  RENDERER] ~";
}

HRESULT WasapiRenderer::QueryInterface(REFIID riid, void **ptr)
{
    if (riid == __uuidof(IMMNotificationClient)) {
        *ptr = static_cast<IMMNotificationClient *>(this);
    }
    else if (riid == __uuidof(IAudioSessionEvents)) {
        *ptr = static_cast<IAudioSessionEvents *>(this);
    }
    else {
        *ptr = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return NOERROR;
}

ULONG WasapiRenderer::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs)); }

ULONG WasapiRenderer::Release()
{
    const auto refcount = static_cast<ULONG>(InterlockedDecrement(&refs));

    if (refcount == 0) {
        delete this;
    }

    return refcount;
}

#endif // _WIN32
