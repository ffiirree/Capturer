#ifndef CAPTURER_TEXTURE_BRIDGE_H
#define CAPTURER_TEXTURE_BRIDGE_H

#include "libcap/ffmpeg-wrapper.h"

class TextureBridge
{
public:
    bool    from(const av::frame& frame);
    int64_t handle();
};

#endif //! CAPTURER_TEXTURE_BRIDGE_H