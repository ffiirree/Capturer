#ifdef _WIN32

#include "libcap/win-wasapi/win-wasapi.h"

#include "libcap/platform.h"
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
    // clang-format off
    switch (layout) {
    case KSAUDIO_SPEAKER_MONO:          return AV_CH_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:        return AV_CH_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD:          return AV_CH_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_2POINT1:       return AV_CH_LAYOUT_SURROUND;
    case KSAUDIO_SPEAKER_SURROUND:      return AV_CH_LAYOUT_4POINT0;
    default:                            return av_get_default_channel_layout(channels);
    }
    // clang-format on
}

std::optional<av::device_t> wasapi::device_info(IMMDevice *dev)
{
    if (!dev) return {};

    winrt::com_ptr<IPropertyStore> props{};
    LPWSTR id        = nullptr;
    PROPVARIANT name = {};
    PROPVARIANT desc = {};

    // Initialize container for property value.
    PropVariantInit(&name);
    defer(PropVariantClear(&name));

    PropVariantInit(&desc);
    defer(PropVariantClear(&desc));

    // Get the endpoint ID string.
    RETURN_NONE_IF_FAILED(dev->GetId(&id));
    defer(CoTaskMemFree(id); id = nullptr);

    RETURN_NONE_IF_FAILED(dev->OpenPropertyStore(STGM_READ, props.put()));

    // Get the endpoint's friendly-name property.
    RETURN_NONE_IF_FAILED(props->GetValue(PKEY_DeviceInterface_FriendlyName, &name));

    // desc
    RETURN_NONE_IF_FAILED(props->GetValue(PKEY_Device_DeviceDesc, &desc));

    // type
    winrt::com_ptr<IMMEndpoint> endpoint{};
    dev->QueryInterface(winrt::guid_of<IMMEndpoint>(), endpoint.put_void());

    EDataFlow data_flow{};
    RETURN_NONE_IF_FAILED(endpoint->GetDataFlow(&data_flow));

    auto type = av::device_type_t::audio;
    // clang-format off
    switch (data_flow) {
    case eRender:   type |= av::device_type_t::sink;                                break;
    case eCapture:  type |= av::device_type_t::source;                              break;
    case eAll:      type |= av::device_type_t::source | av::device_type_t::sink;    break;
    default:                                                                        break;
    }
    // clang-format on

    // state
    DWORD state{};
    RETURN_NONE_IF_FAILED(dev->GetState(&state));

    return av::device_t{
        .name        = probe::util::to_utf8(name.pwszVal),
        .id          = probe::util::to_utf8(id),
        .description = probe::util::to_utf8(desc.pwszVal),
        .type        = type,
        .format      = av::device_format_t::wasapi,
        .state       = static_cast<uint64_t>(state),
    };
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
std::vector<av::device_t> wasapi::endpoints(av::device_type_t type)
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator{};
    winrt::com_ptr<IMMDeviceCollection> collection{};
    std::vector<av::device_t> devices{};

    RETURN_NONE_IF_FAILED(CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr, CLSCTX_ALL,
                                           winrt::guid_of<IMMDeviceEnumerator>(), enumerator.put_void()));

    RETURN_NONE_IF_FAILED(enumerator->EnumAudioEndpoints(
        any(type & av::device_type_t::source) ? eCapture : eRender, DEVICE_STATE_ACTIVE, collection.put()));

    UINT count = 0;
    RETURN_NONE_IF_FAILED(collection->GetCount(&count));
    for (ULONG i = 0; i < count; i++) {
        winrt::com_ptr<IMMDevice> endpoint{};
        RETURN_NONE_IF_FAILED(collection->Item(i, endpoint.put()));

        auto dev = device_info(endpoint.get());
        if (dev.has_value()) {
            devices.emplace_back(dev.value());
        }
    }

    return devices;
}

std::optional<av::device_t> wasapi::default_endpoint(av::device_type_t type)
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator{};
    winrt::com_ptr<IMMDevice> endpoint{};

    RETURN_NONE_IF_FAILED(CoCreateInstance(winrt::guid_of<MMDeviceEnumerator>(), nullptr, CLSCTX_ALL,
                                           winrt::guid_of<IMMDeviceEnumerator>(), enumerator.put_void()));

    RETURN_NONE_IF_FAILED(enumerator->GetDefaultAudioEndpoint(
        any(type & av::device_type_t::source) ? eCapture : eRender, eConsole, endpoint.put()));

    return device_info(endpoint.get());
}

#endif // _WIN32