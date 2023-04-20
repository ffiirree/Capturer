#ifdef _WIN32

#include "win-wasapi.h"
#include <Windows.h>
#include <propsys.h>
#include <functiondiscoverykeys.h>
#include <Audioclient.h>
#include "platform.h"
#include "utils.h"
#include "defer.h"
#include "logging.h"

extern "C" {
#include <libavformat/avformat.h>
}

#define RETURN_NULL_ON_ERROR(hres) if (FAILED(hres)) { return {}; }
#define RETURN_ON_ERROR(hres)  if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

uint64_t wasapi::to_ffmpeg_channel_layout(DWORD layout, int channels)
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

std::optional<avdevice_t> wasapi::device_info(IMMDevice* dev)
{
    if (!dev) return {};

    IPropertyStore* props = nullptr;
    LPWSTR id = nullptr;

    // Get the endpoint ID string.
    RETURN_NULL_ON_ERROR(dev->GetId(&id));
    defer(CoTaskMemFree(id); id = nullptr);

    RETURN_NULL_ON_ERROR(dev->OpenPropertyStore(STGM_READ, &props));
    defer(SAFE_RELEASE(props));

    PROPVARIANT varName;
    // Initialize container for property value.
    PropVariantInit(&varName);
    defer(PropVariantClear(&varName));

    // Get the endpoint's friendly-name property.
    RETURN_NULL_ON_ERROR(props->GetValue(PKEY_Device_FriendlyName, &varName));

    DWORD state{};
    RETURN_NULL_ON_ERROR(dev->GetState(&state));

    return avdevice_t{
        platform::util::to_utf8(varName.pwszVal),
        platform::util::to_utf8(id),
        AVMEDIA_TYPE_AUDIO,
        avdevice_t::UNKNOWN,
        static_cast<uint64_t>(state)
    };
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
std::vector<avdevice_t> wasapi::endpoints(avdevice_t::io_t io_type)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;
    std::vector<avdevice_t> devices;

    // RETURN_NULL_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    // defer(CoUninitialize());

    RETURN_NULL_ON_ERROR(CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    ));
    defer(SAFE_RELEASE(enumerator));

    RETURN_NULL_ON_ERROR(enumerator->EnumAudioEndpoints(
        (io_type == avdevice_t::SOURCE) ? eCapture : eRender,
        DEVICE_STATE_ACTIVE,
        &collection
    ));
    defer(SAFE_RELEASE(collection));

    UINT count = 0;
    IMMDevice* endpoint = nullptr;
    RETURN_NULL_ON_ERROR(collection->GetCount(&count));
    for (ULONG i = 0; i < count; i++) {
        // Get pointer to endpoint number i.
        RETURN_NULL_ON_ERROR(collection->Item(i, &endpoint));
        defer(SAFE_RELEASE(endpoint));

        auto dev = device_info(endpoint);
        if (dev) {
            dev.value().io_type = io_type;
            devices.emplace_back(dev.value());
        }
    }

    return devices;
}

std::optional<avdevice_t> wasapi::default_endpoint(avdevice_t::io_t io_type)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IPropertyStore* props = nullptr;

    RETURN_NULL_ON_ERROR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    defer(CoUninitialize());

    RETURN_NULL_ON_ERROR(CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    ));
    defer(SAFE_RELEASE(enumerator));

    RETURN_NULL_ON_ERROR(enumerator->GetDefaultAudioEndpoint(
        (io_type == avdevice_t::SOURCE) ? eCapture : eRender,
        eConsole,
        &endpoint
    ));
    defer(SAFE_RELEASE(endpoint));


    // Print endpoint friendly name and endpoint ID.
    return device_info(endpoint);
}

#endif // _WIN32