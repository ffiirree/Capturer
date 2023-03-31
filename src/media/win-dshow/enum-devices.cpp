#include "enum-devices.h"

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


static std::vector<std::pair<std::wstring, std::wstring>> DisplayDeviceInformation(IEnumMoniker* pEnum)
{
    IMoniker* moniker = nullptr;
    std::vector<std::pair<std::wstring, std::wstring>> list;

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

        list.emplace_back(name, id);
    }

    return list;
}

static std::vector<std::pair<std::wstring, std::wstring>> EnumerateDevices(REFGUID category)
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


    return DisplayDeviceInformation(enum_moniker);
}

std::vector<std::pair<std::wstring, std::wstring>> enum_video_devices()
{
    return EnumerateDevices(CLSID_VideoInputDeviceCategory);
}

std::vector<std::pair<std::wstring, std::wstring>> enum_audio_devices()
{
    return EnumerateDevices(CLSID_AudioInputDeviceCategory);
}


#endif // !_WIN32