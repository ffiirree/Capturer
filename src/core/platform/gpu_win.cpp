#ifdef _WIN32

#include "platform.h"
#include <windows.h>

namespace platform::gpu
{
    gpu_info_t info()
    {
        return {};
    }
}

#endif // _WIN32