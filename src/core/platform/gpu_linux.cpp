#ifdef __linux__

#include "platform.h"

namespace platform::gpu
{
    std::vector<gpu_info_t> info()
    {
        return {};
    }
}

#endif // __linux__