#ifndef CAPTURER_UTILS_H
#define CAPTURER_UTILS_H

#include "probe/enum.h"

#ifdef _WIN32
#define RETURN_NONE_IF_FAILED(HR)                                                                          \
    if (FAILED(HR)) return {};

#define RETURN_NEGV_IF_FAILED(HR)                                                                          \
    if (FAILED(HR)) return -1;
#endif

enum class graph_t
{
    none,
    rectangle = 0x0001,
    ellipse   = 0x0002,
    arrow     = 0x0004,
    line      = 0x0008,
    curve     = 0x0010,
    text      = 0x0020,
    mosaic    = 0x0040,
    eraser    = 0x0080,

    ENABLE_BITMASK_OPERATORS(),
};

enum PaintType : uint32_t
{
    UNMODIFIED     = 0x0000,
    UPDATE_MASK    = 0x0001,
    DRAW_MODIFYING = 0x0010,
    DRAW_MODIFIED  = 0x0020 | UPDATE_MASK,
    DRAW_FINISHED  = 0x0040 | UPDATE_MASK,
    REPAINT_ALL    = 0x0100 | DRAW_MODIFIED | DRAW_FINISHED,
};

#endif //! CAPTURER_UTILS_H
