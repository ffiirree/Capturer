#ifndef CAPTURER_PLATFORM_H
#define CAPTURER_PLATFORM_H

#ifdef _WIN32
// clang-format off
#define RETURN_IF_FAILED(HR)        if (auto hres = (HR); FAILED(hres)) return hres;

#define RETURN_NONE_IF_FAILED(HR)   if (FAILED(HR)) return {};

#define RETURN_NEGV_IF_FAILED(HR)   if (FAILED(HR)) return -1;
// clang-format on
#endif

#endif //! CAPTURER_PLATFORM_H
