#ifndef CAPTURER_UTILS_H
#define CAPTURER_UTILS_H

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
    image     = 0x0020,
    counter   = 0x0040,
    text      = 0x0080,
    mosaic    = 0x0100,
    eraser    = 0x0200,
};

#endif //! CAPTURER_UTILS_H
