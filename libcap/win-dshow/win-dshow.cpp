#include "libcap/win-dshow/win-dshow.h"

#ifdef _WIN32

#include "logging.h"
#include "libcap/platform.h"
#include "probe/defer.h"
#include "probe/util.h"

#include <dshow.h>
#include <fmt/format.h>
#include <windows.h>
#include <winrt/base.h>

std::vector<av::device_t> dshow::video_devices()
{
    winrt::com_ptr<ICreateDevEnum> dev_enum{};
    RETURN_NONE_IF_FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                           winrt::guid_of<ICreateDevEnum>(), dev_enum.put_void()));

    // Create an enumerator for the category.
    winrt::com_ptr<IEnumMoniker> enum_moniker{};
    if (dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, enum_moniker.put(), 0) != S_OK)
        return {};

    std::vector<av::device_t> list;
    for (winrt::com_ptr<IMoniker> moniker{}; enum_moniker->Next(1, moniker.put(), nullptr) == S_OK;
         moniker = nullptr) {
        winrt::com_ptr<IPropertyBag> props{};
        if (FAILED(moniker->BindToStorage(nullptr, nullptr, winrt::guid_of<IPropertyBag>(),
                                          props.put_void()))) {
            continue;
        }

        VARIANT var;
        VariantInit(&var);
        defer(VariantClear(&var));

        std::wstring name{}, desc{}, id{};

        // Get description and friendly name.
        if (SUCCEEDED(props->Read(L"Description", &var, 0))) desc = var.bstrVal;
        if (SUCCEEDED(props->Read(L"FriendlyName", &var, 0))) name = var.bstrVal;
        if (SUCCEEDED(props->Read(L"DevicePath", &var, 0))) id = var.bstrVal;

        list.emplace_back(av::device_t{
            .name        = probe::util::to_utf8(name),
            .id          = probe::util::to_utf8(id),
            .description = probe::util::to_utf8(desc),
            .type        = av::device_type_t::video | av::device_type_t::source,
            .format      = av::device_format_t::dshow,
        });
    }

    return list;
}

#endif // !_WIN32