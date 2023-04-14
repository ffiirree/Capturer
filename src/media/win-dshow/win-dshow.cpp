#include "win-dshow.h"

#ifdef _WIN32

#include <windows.h>
#include <dshow.h>
#include <fmt/format.h>
#include "logging.h"
#include "utils.h"
#include "defer.h"

#define RETURN_NULL_ON_ERROR(hres) if (FAILED(hres)) { return {}; }
#define RETURN_ON_ERROR(hres)  if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != nullptr) { (punk)->Release(); (punk) = nullptr; }


static std::vector<avdevice_t> DisplayDeviceInformation(IEnumMoniker* pEnum, enum AVMediaType mt)
{
    IMoniker* moniker = nullptr;
    std::vector<avdevice_t> list;

    while (pEnum->Next(1, &moniker, NULL) == S_OK)
    {
        IPropertyBag* pPropBag;
        HRESULT hr = moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            moniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        std::wstring name, id;

        // Get description or friendly name.
        hr = pPropBag->Read(L"Description", &var, 0);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"FriendlyName", &var, 0);
        }
        if (SUCCEEDED(hr))
        {
            name = var.bstrVal;
            VariantClear(&var);
        }

        hr = pPropBag->Write(L"FriendlyName", &var);

        // WaveInID applies only to audio capture devices.
        hr = pPropBag->Read(L"WaveInID", &var, 0);
        if (SUCCEEDED(hr))
        {
            id = std::to_wstring(var.lVal);
            VariantClear(&var);
        }

        hr = pPropBag->Read(L"DevicePath", &var, 0);
        if (SUCCEEDED(hr))
        {
            // The device path is not intended for display.
            id = var.bstrVal;
            VariantClear(&var);
        }

        pPropBag->Release();
        moniker->Release();

        list.emplace_back(avdevice_t{
            platform::util::to_utf8(name),
            platform::util::to_utf8(id),
            mt,
            avdevice_t::SOURCE
        });
    }

    return list;
}

static std::vector<avdevice_t> EnumerateDevices(REFGUID category)
{
    //RETURN_NULL_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    //defer(CoUninitialize());

    ICreateDevEnum* dev_enum = nullptr;
    RETURN_NULL_ON_ERROR(CoCreateInstance(
        CLSID_SystemDeviceEnum,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dev_enum)
    ));
    defer(SAFE_RELEASE(dev_enum));

    IEnumMoniker* enum_moniker = nullptr;
    defer(SAFE_RELEASE(enum_moniker));

    // Create an enumerator for the category.
    if (dev_enum->CreateClassEnumerator(category, &enum_moniker, 0) != S_OK) {
        return {};
    }

    enum AVMediaType mt{};
    if (category == CLSID_VideoInputDeviceCategory)
        mt = AVMEDIA_TYPE_VIDEO;
    else if (category == CLSID_AudioInputDeviceCategory)
        mt = AVMEDIA_TYPE_AUDIO;

    return DisplayDeviceInformation(enum_moniker, mt);
}

std::vector<avdevice_t> dshow::video_devices()
{
    return EnumerateDevices(CLSID_VideoInputDeviceCategory);
}

std::vector<avdevice_t> dshow::audio_devices()
{
    return EnumerateDevices(CLSID_AudioInputDeviceCategory);
}

#endif // !_WIN32