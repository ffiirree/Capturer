#include "win-dshow.h"

#ifdef _WIN32

#include "logging.h"
#include "probe/defer.h"
#include "probe/util.h"
#include "utils.h"

#include <dshow.h>
#include <fmt/format.h>
#include <windows.h>
#include <winrt/base.h>

static std::vector<av::device_t> DisplayDeviceInformation(IEnumMoniker *pEnum, av::device_type_t type)
{
    IMoniker *moniker = nullptr;
    std::vector<av::device_t> list;

    while (pEnum->Next(1, &moniker, NULL) == S_OK) {
        winrt::com_ptr<IPropertyBag> props{};
        if (FAILED(moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(props.put())))) {
            moniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);
        defer(VariantClear(&var));

        std::wstring name, id;

        // Get description or friendly name.
        auto hr = props->Read(L"Description", &var, 0);
        if (FAILED(hr)) {
            hr = props->Read(L"FriendlyName", &var, 0);
        }
        if (SUCCEEDED(hr)) {
            name = var.bstrVal;
        }

        hr = props->Write(L"FriendlyName", &var);

        // WaveInID applies only to audio capture devices.
        if (SUCCEEDED(props->Read(L"WaveInID", &var, 0))) {
            id = std::to_wstring(var.lVal);
        }

        if (SUCCEEDED(props->Read(L"DevicePath", &var, 0))) {
            // The device path is not intended for display.
            id = var.bstrVal;
        }

        moniker->Release();

        list.push_back({
            .name   = probe::util::to_utf8(name),
            .id     = probe::util::to_utf8(id),
            .type   = type,
            .format = av::device_format_t::dshow,
        });
    }

    return list;
}

static std::vector<av::device_t> EnumerateDevices(REFGUID category)
{
    winrt::com_ptr<ICreateDevEnum> dev_enum{};
    RETURN_NONE_IF_FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(dev_enum.put())));

    // Create an enumerator for the category.
    winrt::com_ptr<IEnumMoniker> enum_moniker{};
    RETURN_NONE_IF_FAILED(dev_enum->CreateClassEnumerator(category, enum_moniker.put(), 0));

    av::device_type_t mt{};
    if (category == CLSID_VideoInputDeviceCategory)
        mt = av::device_type_t::video | av::device_type_t::source;

    else if (category == CLSID_AudioInputDeviceCategory)
        mt = av::device_type_t::audio;

    return DisplayDeviceInformation(enum_moniker.get(), mt);
}

std::vector<av::device_t> dshow::video_devices()
{
    return EnumerateDevices(CLSID_VideoInputDeviceCategory);
}

std::vector<av::device_t> dshow::audio_devices()
{
    return EnumerateDevices(CLSID_AudioInputDeviceCategory);
}

#endif // !_WIN32