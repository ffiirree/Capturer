#ifndef CAPTURER_WASAPI_RENDER_H
#define CAPTURER_WASAPI_RENDER_H

#ifdef _WIN32

#include "libcap/audio-renderer.h"
#include "libcap/devices.h"

#include <atomic>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <mutex>
#include <propsys.h>
#include <thread>
#include <Windows.h>
#include <winrt/base.h>

class WasapiRenderer : public AudioRenderer, public IMMNotificationClient, public IAudioSessionEvents
{
public:
    WasapiRenderer() { InterlockedIncrement(&refs); }

    ~WasapiRenderer() override
    {
        ::SetEvent(STOP_EVENT.get());

        if (thread_.joinable()) thread_.join();

        if (wfex_) CoTaskMemFree(wfex_);

        InterlockedDecrement(&refs);
    }

    int  open(const std::string& name, RenderFlags flags) override;
    bool ready() const override { return ready_; }

    int start() override;
    int reset() override;
    int stop() override;

    int  mute(bool) override;
    bool muted() const override;

    int   setVolume(float) override;
    float volume() const override;

    int  pause() override;
    int  resume() override;
    bool paused() const override;

    av::aformat_t format() const override { return format_; }

    uint32_t buffer_size() const override { return buffer_frames_; }

protected:
    long refs = 0;

    // IUnknown
    HRESULT QueryInterface(REFIID riid, void **ppvObject) override;
    ULONG   AddRef() override;
    ULONG   Release() override;

    // IMMNotificationClient
    HRESULT OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    HRESULT OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    // IAudioSessionEvents
    HRESULT OnDisplayNameChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    HRESULT OnIconPathChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    HRESULT OnSimpleVolumeChanged(float, BOOL, LPCGUID) override { return S_OK; }
    HRESULT OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) override { return S_OK; }
    HRESULT OnGroupingParamChanged(LPCGUID, LPCGUID) override { return S_OK; }
    HRESULT OnStateChanged(AudioSessionState) override { return S_OK; }
    HRESULT OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override;

    //
    void    InitializeWaveFormat(WAVEFORMATEX *);
    HRESULT InitializeAudioEngine();
    HRESULT InitializeStreamSwitch();

    // handler
    HRESULT RequestEventHandler(std::chrono::nanoseconds);
    HRESULT SwitchEventHandler();

private:
    std::atomic<bool> ready_{ false };
    std::atomic<bool> running_{ false };

    RenderFlags flags_{};

    av::aformat_t format_{};
    av::device_t  devinfo_{};

    std::jthread thread_{};

    // WASAPI Render@{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator_{};
    winrt::com_ptr<IMMDevice>           endpoint_{};

    mutable std::mutex mtx_; // for stream switch

    winrt::com_ptr<IAudioClient>         audio_client_{};
    winrt::com_ptr<IAudioRenderClient>   render_{};
    winrt::com_ptr<ISimpleAudioVolume>   volume_{};
    winrt::com_ptr<IAudioSessionControl> session_{};

    WAVEFORMATEX *wfex_{ nullptr };

    UINT32 buffer_frames_{ 0 };

    std::atomic<bool> switching_{ false };

    winrt::handle REQUEST_EVENT{ nullptr };
    winrt::handle STOP_EVENT{ nullptr };
    winrt::handle SWITCH_EVENT{ nullptr };
    winrt::handle SWITCH_COMPLETE_EVENT{ nullptr };
    // @}
};
#endif // _WIN32

#endif //! CAPTURER_WASAPI_RENDER_H
