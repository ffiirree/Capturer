#ifndef CAPTURER_PLATFORM_H
#define CAPTURER_PLATFORM_H

#ifdef _WIN32
#define RETURN_NONE_IF_FAILED(HR)                                                                          \
    if (FAILED(HR)) return {};

#define RETURN_NEGV_IF_FAILED(HR)                                                                          \
    if (FAILED(HR)) return -1;
#endif

#endif //! CAPTURER_PLATFORM_H
