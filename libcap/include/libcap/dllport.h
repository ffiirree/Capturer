#ifndef CAPTURER_DLLPORT_H
#define CAPTURER_DLLPORT_H

// clang-format off
#ifdef _MSC_VER
#    ifdef CAPTURER_SHARED
#        ifdef CAPTURER_BUILDING
#            define CAPTURER_API __declspec(dllexport)
#        else
#            define CAPTURER_API __declspec(dllimport)
#        endif
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    if defined(CAPTURER_SHARED) || defined(CAPTURER_BUILDING)
#        define CAPTURER_API __attribute__((visibility("default")))
#    endif
#endif

#ifndef CAPTURER_API
#    define CAPTURER_API
#endif
// clang-format on

#endif //! CAPTURER_DLLPORT_H