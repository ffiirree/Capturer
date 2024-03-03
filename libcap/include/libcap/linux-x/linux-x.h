#ifndef CAPTURER_LINUX_X_H
#define CAPTURER_LINUX_X_H

#ifdef __linux__

#include <xcb/xcb.h>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace x
{
    AVPixelFormat from_xcb_pixmap_format(xcb_connection_t *conn, int depth);
} // namespace x

#endif

#endif //! CAPTURER_LINUX_X_H