#ifdef _WIN32

#include "enum-wasapi.h"
#include <Windows.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <functiondiscoverykeys.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <optional>
#include <Audioclient.h>
#include "utils.h"
#include "logging.h"

#define RETURN_NULL_ON_ERROR(hres) if (FAILED(hres)) { return {}; }
#define RETURN_ON_ERROR(hres)  if (FAILED(hres)) { return -1; }
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

size_t try_wchar_to_utf8_ptr(const wchar_t* in_ptr, size_t in_len, char* out_ptr, size_t out_len)
{
    return WideCharToMultiByte(CP_UTF8, 0,
        in_ptr, (int)in_len,
        out_ptr, (int)out_len,
        nullptr, nullptr);
}

std::string wchar_to_utf8(const wchar_t* ptr, size_t len)
{
    if (!ptr) return { };
    if (!len) len = wcslen(ptr);

    size_t u8len = try_wchar_to_utf8_ptr(ptr, len, nullptr, 0);

    char* buffer = new char[u8len + 1]{ 0 }; defer(delete[]buffer);

    u8len = try_wchar_to_utf8_ptr(ptr, len, buffer, u8len + 1);
    buffer[u8len] = 0;

    return { buffer, u8len };
}

// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
std::vector<std::pair<std::string, std::string>> enum_audio_endpoints(bool is_input)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;
    IMMDevice* endpoint = nullptr;
    LPWSTR id = nullptr;
    IPropertyStore* props = nullptr;
    UINT count = 0;
    std::vector<std::pair<std::string, std::string>> devices;

    RETURN_NULL_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    defer(CoUninitialize());

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
        devices.emplace_back(
            wchar_to_utf8(varName.pwszVal, wcslen(varName.pwszVal)),
            wchar_to_utf8(id, wcslen(id))
        );
    }

    return devices;
}

std::optional<std::pair<std::string, std::string>> default_audio_endpoint(bool is_input)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;
    IMMDevice* endpoint = nullptr;
    LPWSTR id = nullptr;
    IPropertyStore* props = nullptr;

    RETURN_NULL_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
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
    return std::pair{
            wchar_to_utf8(varName.pwszVal, wcslen(varName.pwszVal)),
            wchar_to_utf8(id, wcslen(id))
    };
}

#endif // _WIN32