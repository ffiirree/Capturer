#ifdef _WIN32

#include "platform.h"
#include <windows.h>
#include <DXGI.h>

namespace platform::gpu
{
    std::vector<gpu_info_t> info()
    {
        IDXGIFactory* factory = nullptr;
        if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&factory)))) {
            return {};
        }

        std::vector<gpu_info_t> cards;

        UINT idx = 0;
        IDXGIAdapter *adapter = nullptr;
        while (factory->EnumAdapters(idx, &adapter) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC desc{};
            adapter->GetDesc(&desc);

            cards.emplace_back(gpu_info_t{ 
                desc.Description, 
                vendor_cast(desc.VendorId),
                desc.DedicatedVideoMemory, 
                desc.SharedSystemMemory, 
                0                           // TODO:
            });

            ++idx;
        }
        return cards;
    }
}

#endif // _WIN32