#ifndef UTILS_H
#define UTILS_H

#include <memory>
#include <utility>
#include <vector>

#ifndef st
#define st(X)                                                                                              \
    do {                                                                                                   \
        X                                                                                                  \
    } while (0)
#endif

#ifdef _WIN32
#define RETURN_NONE_IF_FAILED(HR)                                                                          \
    if (FAILED(HR)) return {};

#define RETURN_NEGV_IF_FAILED(HR)                                                                          \
    if (FAILED(HR)) return -1;

#define SAFE_RELEASE(COMPTR)                                                                               \
    if ((COMPTR) != nullptr) {                                                                             \
        (COMPTR)->Release();                                                                               \
        (COMPTR) = nullptr;                                                                                \
    }
#endif

enum Graph : uint32_t
{
    NONE,
    RECTANGLE   = 0x0001,
    ELLIPSE     = 0x0002,
    ARROW       = 0x0004,
    LINE        = 0x0008,
    CURVES      = 0x0010,
    TEXT        = 0x0020,
    MOSAIC      = 0x0040,
    ERASER      = 0x0080,
    BROKEN_LINE = 0x0100,
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

#endif // UTILS_H
