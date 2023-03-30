#ifdef __linux__

#include "platform.h"
#include "defer.h"

namespace platform::display {
    std::vector<display_t> displays()
    {
        return {};
    }
}

#endif // __linux__
