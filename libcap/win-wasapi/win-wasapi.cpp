#ifdef _WIN32

#include "libcap/win-wasapi/win-wasapi.h"

#include "libcap/media.h"
#include "logging.h"

#include <Audioclient.h>
#include <functiondiscoverykeys.h>
#include <probe/defer.h>
#include <probe/util.h>
#include <propsys.h>
#include <Windows.h>
#include <winrt/base.h>

extern "C" {
#include <libavformat/avformat.h>
}

uint64_t wasapi::to_ffmpeg_channel_layout(DWORD layout, int channels)
{
    switch (layout) {
    case KSAUDIO_SPEAKER_MONO:     return AV_CH_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:   return AV_CH_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD:     return AV_CH_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_2POINT1:  return AV_CH_LAYOUT_SURROUND;
    case KSAUDIO_SPEAKER_SURROUND: return AV_CH_LAYOUT_4POINT0;
    default:                       return av::default_channel_layout(channels);
    }
}

std::optional<av::device_t> wasapi::device_info(IMMDevice *dev)
{
    if (!dev) return {};

    winrt::com_ptr<IPropertyStore> props{};
    LPWSTR                         id   = nullptr;
    PROPVARIANT                    name = {};
    PROPVARIANT                    desc = {};

    try {
        // Initialize container for property value.
        PropVariantInit(&name);
        defer(winrt::check_hresult(PropVariantClear(&name)));

        PropVariantInit(&desc);
        defer(winrt::check_hresult(PropVariantClear(&desc)));

        // Get the endpoint ID string.
        winrt::check_hresult(dev->GetId(&id));
        defer(CoTaskMemFree(id); id = nullptr);

        winrt::check_hresult(dev->OpenPropertyStore(STGM_READ, props.put()));

        // friendly-name & desc
        winrt::check_hresult(props->GetValue(PKEY_DeviceInterface_FriendlyName, &name));
        winrt::check_hresult(props->GetValue(PKEY_Device_DeviceDesc, &desc));

        // type
        winrt::com_ptr<IMMEndpoint> endpoint{};
        winrt::check_hresult(dev->QueryInterface(winrt::guid_of<IMMEndpoint>(), endpoint.put_void()));

        EDataFlow data_flow{};
        winrt::check_hresult(endpoint->GetDataFlow(&data_flow));

        auto type = av::device_type_t::audio;
        switch (data_flow) {
        case eRender:  type |= av::device_type_t::sink; break;
        case eCapture: type |= av::device_type_t::source; break;
        case eAll:     type |= av::device_type_t::source | av::device_type_t::sink; break;
        default:       break;
        }

        // state
        DWORD state{};
        winrt::check_hresult(dev->GetState(&state));

        return av::device_t{
            .name        = probe::util::to_utf8(name.pwszVal),
            .id          = probe::util::to_utf8(id),
            .description = probe::util::to_utf8(desc.pwszVal),
            .type        = type,
            .format      = av::device_format_t::WASAPI,
            .state       = static_cast<uint64_t>(state),
        };
    }
    catch (const winrt::hresult_error& e) {
        loge("[     WASAPI] {:#x}: {}", static_cast<uint32_t>(e.code()),
             probe::util::to_utf8(e.message().c_str()));
        return {};
    }
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
std::vector<av::device_t> wasapi::endpoints(av::device_type_t type)
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator{};
    winrt::com_ptr<IMMDeviceCollection> collection{};
    std::vector<av::device_t>           devices{};

    try {
        winrt::check_hresult(::CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr, CLSCTX_ALL,
                                                winrt::guid_of<IMMDeviceEnumerator>(),
                                                enumerator.put_void()));

        winrt::check_hresult(
            enumerator->EnumAudioEndpoints(any(type & av::device_type_t::source) ? eCapture : eRender,
                                           DEVICE_STATE_ACTIVE, collection.put()));

        UINT count = 0;
        winrt::check_hresult(collection->GetCount(&count));
        for (ULONG i = 0; i < count; i++) {
            winrt::com_ptr<IMMDevice> endpoint{};
            winrt::check_hresult(collection->Item(i, endpoint.put()));

            if (auto dev = device_info(endpoint.get()); dev.has_value()) {
                devices.emplace_back(dev.value());
            }
        }
    }
    catch (const winrt::hresult_error& e) {
        loge("[     WASAPI] {:#x}: {}", static_cast<uint32_t>(e.code()),
             probe::util::to_utf8(e.message().c_str()));
    }

    return devices;
}

std::optional<av::device_t> wasapi::default_endpoint(av::device_type_t type)
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator{};
    winrt::com_ptr<IMMDevice>           endpoint{};

    if (FAILED(CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr, CLSCTX_ALL,
                                winrt::guid_of<IMMDeviceEnumerator>(), enumerator.put_void())))
        return std::nullopt;

    if (FAILED(enumerator->GetDefaultAudioEndpoint(
            any(type & av::device_type_t::source) ? eCapture : eRender, eConsole, endpoint.put())))
        return std::nullopt;

    return device_info(endpoint.get());
}

#endif // _WIN32