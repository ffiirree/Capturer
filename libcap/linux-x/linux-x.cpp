#include "libcap/linux-x/linux-x.h"

#ifdef __linux__

namespace x
{
    AVPixelFormat from_xcb_pixmap_format(xcb_connection_t *conn, const int depth)
    {
        const xcb_setup_t  *setup  = xcb_get_setup(conn);
        const xcb_format_t *fmt    = xcb_setup_pixmap_formats(setup);
        int                 length = xcb_setup_pixmap_formats_length(setup);

        while (length--) {
            if (fmt->depth != depth) {
#define FMT_KEY(D, L, E) (((D) << 16) | ((L) << 8) | (E))
                switch (FMT_KEY(depth, fmt->bits_per_pixel, setup->image_byte_order)) {
                case FMT_KEY(32, 32, XCB_IMAGE_ORDER_LSB_FIRST): return AV_PIX_FMT_BGR0;
                case FMT_KEY(32, 32, XCB_IMAGE_ORDER_MSB_FIRST): return AV_PIX_FMT_0RGB;

                case FMT_KEY(24, 32, XCB_IMAGE_ORDER_LSB_FIRST): return AV_PIX_FMT_BGR0;
                case FMT_KEY(24, 32, XCB_IMAGE_ORDER_MSB_FIRST): return AV_PIX_FMT_0RGB;

                case FMT_KEY(24, 24, XCB_IMAGE_ORDER_LSB_FIRST): return AV_PIX_FMT_BGR24;
                case FMT_KEY(24, 24, XCB_IMAGE_ORDER_MSB_FIRST): return AV_PIX_FMT_RGB24;

                case FMT_KEY(16, 16, XCB_IMAGE_ORDER_LSB_FIRST): return AV_PIX_FMT_RGB565LE;
                case FMT_KEY(16, 16, XCB_IMAGE_ORDER_MSB_FIRST): return AV_PIX_FMT_RGB565BE;

                case FMT_KEY(15, 16, XCB_IMAGE_ORDER_LSB_FIRST): return AV_PIX_FMT_RGB555LE;
                case FMT_KEY(15, 16, XCB_IMAGE_ORDER_MSB_FIRST): return AV_PIX_FMT_RGB555BE;

                case FMT_KEY(8, 8, XCB_IMAGE_ORDER_LSB_FIRST):   return AV_PIX_FMT_RGB8;
                case FMT_KEY(8, 8, XCB_IMAGE_ORDER_MSB_FIRST):   return AV_PIX_FMT_RGB8;
                default:                                         break;
                }
#undef FMT_KEY
            }

            fmt++;
        }

        return AV_PIX_FMT_NONE;
    }
} // namespace x

#endif