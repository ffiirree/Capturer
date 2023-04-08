#ifdef _WIN32

#include "enum-wasapi.h"
#include <Windows.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <functiondiscoverykeys.h>
#include <Audioclient.h>
#include "utils.h"
#include "defer.h"
#include "logging.h"

#define RETURN_NULL_ON_ERROR(hres) if (FAILED(hres)) { return {}; }
#define RETURN_ON_ERROR(hres)  if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
std::vector<std::pair<std::wstring, std::wstring>> enum_audio_endpoints(bool is_input)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;
    IMMDevice* endpoint = nullptr;
    LPWSTR id = nullptr;
    IPropertyStore* props = nullptr;
    UINT count = 0;
    std::vector<std::pair<std::wstring, std::wstring>> devices;

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
        is_input ? eCapture : eRender,
        DEVICE_STATE_ACTIVE,
        &collection
    ));
    defer(SAFE_RELEASE(collection));

    RETURN_NULL_ON_ERROR(collection->GetCount(&count));
    for (ULONG i = 0; i < count; i++) {
        // Get pointer to endpoint number i.
        RETURN_NULL_ON_ERROR(collection->Item(i, &endpoint));
        defer(SAFE_RELEASE(endpoint));

        // Get the endpoint ID string.
        RETURN_NULL_ON_ERROR(endpoint->GetId(&id));
        defer(CoTaskMemFree(id); id = nullptr);

        RETURN_NULL_ON_ERROR(endpoint->OpenPropertyStore(STGM_READ, &props));
        defer(SAFE_RELEASE(props));

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);
        defer(PropVariantClear(&varName));

        // Get the endpoint's friendly-name property.
        RETURN_NULL_ON_ERROR(props->GetValue(PKEY_Device_FriendlyName, &varName));

        // Print endpoint friendly name and endpoint ID.
        devices.emplace_back(varName.pwszVal, id);
    }

    return devices;
}

std::optional<std::pair<std::wstring, std::wstring>> default_audio_endpoint(bool is_input)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    LPWSTR id = nullptr;
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
        is_input ? eCapture : eRender,
        eConsole,
        &endpoint
    ));
    defer(SAFE_RELEASE(endpoint));

    RETURN_NULL_ON_ERROR(endpoint->GetId(&id));
    defer(CoTaskMemFree(id); id = nullptr);

    RETURN_NULL_ON_ERROR(endpoint->OpenPropertyStore(STGM_READ, &props));
    defer(SAFE_RELEASE(props));

    PROPVARIANT varName;
    // Initialize container for property value.
    PropVariantInit(&varName);
    defer(PropVariantClear(&varName));

    // Get the endpoint's friendly-name property.
    RETURN_NULL_ON_ERROR(props->GetValue(PKEY_Device_FriendlyName, &varName));

    // Print endpoint friendly name and endpoint ID.
    return std::pair{ varName.pwszVal,  id };
}

#endif // _WIN32