#include "libcap/linux-x/xshm-capturer.h"

#ifdef __linux__

#include "libcap/linux-x/linux-x.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <probe/defer.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
}

static AVBufferRef *xshm_alloc(void *opaque, const int size)
{
    const auto          xconn = static_cast<xcb_connection_t *>(opaque);
    const xcb_shm_seg_t xseg  = ::xcb_generate_id(xconn);

    // get shared memory segment
    const auto shmid = ::shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
    if (shmid == -1) return nullptr;

    ::xcb_shm_attach(xconn, xseg, shmid, 0);

    // attach shared memory segment
    auto data = static_cast<uint8_t *>(::shmat(shmid, nullptr, 0));
    if (reinterpret_cast<intptr_t>(data) == -1 || !data) return nullptr;

    ::shmctl(shmid, IPC_RMID, nullptr);

    const auto ffbuf = av_buffer_create(
        data, size, [](auto, uint8_t *data) { ::shmdt(data); },
        reinterpret_cast<void *>(static_cast<ptrdiff_t>(xseg)), 0);
    if (!ffbuf) ::shmdt(data);

    return ffbuf;
}

int XshmCapturer::xfixes_draw_cursor(av::frame& frame) const
{
    const auto ci = xcb_xfixes_get_cursor_image_reply(conn_, ::xcb_xfixes_get_cursor_image(conn_), nullptr);
    if (!ci) {
        LOG(ERROR) << "[ XCB-XFIXES] failed to get cursor image";
        return -1;
    }
    defer(free(ci));

    const auto cursor = xcb_xfixes_get_cursor_image_cursor_image(ci);
    if (!cursor) return -1;

    // (x, y) world cusor position, (xhot, yhot) offset in cursor image
    const auto cx = ci->x - ci->xhot;
    const auto cy = ci->y - ci->yhot;

    // intersection
    const auto x = std::max(cx, left);
    const auto y = std::max(cy, top);

    const auto w = std::min(cx + ci->width, left + vfmt.width) - x;
    const auto h = std::min(cy + ci->height, top + vfmt.height) - y;

    //
    const int pbytes = bpp_ / 8;
    auto      fptr   = frame->data[0] + (x - left) * pbytes + (y - top) * frame->linesize[0];
    auto      cptr   = cursor + (x - cx) + (y - cy) * ci->width;

    for (int i = 0; i < h; ++i, fptr += frame->linesize[0], cptr += ci->width) {
        auto fline = fptr;
        auto cline = cptr;
        for (int j = 0; j < w; ++j, fline += pbytes, ++cline) {

            const uint8_t r = (*cline >> 0) & 0xff;
            const uint8_t g = (*cline >> 8) & 0xff;
            const uint8_t b = (*cline >> 16) & 0xff;
            const uint8_t a = (*cline >> 24) & 0xff;

            switch (a) {
            case 0: break;
            case 255:
                fline[0] = r;
                fline[1] = g;
                fline[2] = b;
                break;
            default:
                fline[0] = r + (fline[0] * (255 - a) + 127) / 255;
                fline[1] = g + (fline[1] * (255 - a) + 127) / 255;
                fline[2] = b + (fline[2] * (255 - a) + 127) / 255;
                break;
            }
        }
    }

    return 0;
}

int XshmCapturer::open(const std::string& name, std::map<std::string, std::string>)
{
    int nb_screen{ -1 };
    conn_ = ::xcb_connect(":1.0", &nb_screen);

    if (const auto ret = ::xcb_connection_has_error(conn_); ret) {
        LOG(ERROR) << fmt::format("[    LINUX-X] cannot open dispaly {}, error {}", name, ret);
        return -1;
    }

    const auto setup = ::xcb_get_setup(conn_);

    for (auto iter = ::xcb_setup_roots_iterator(setup); iter.rem > 0; ::xcb_screen_next(&iter)) {
        if (!nb_screen) {
            screen_ = iter.data;
            break;
        }
        nb_screen--;
    }

    if (!screen_) {
        LOG(ERROR) << fmt::format("[    LINUX-X] the screen {} does not exist", nb_screen);
        return -1;
    }

    wid_ = screen_->root;

    const auto geo = ::xcb_get_geometry_reply(conn_, ::xcb_get_geometry(conn_, wid_), nullptr);
    defer(::free(geo));
    if (!geo) {
        LOG(ERROR) << fmt::format("[    LINUX-X] cannot find the window 0x{:08X}", wid_);
        return -1;
    }

    left = std::max<int>(left, geo->x);
    top  = std::max<int>(top, geo->y);

    vfmt.width  = vfmt.width <= 0 ? geo->width : vfmt.width;
    vfmt.height = vfmt.height <= 0 ? geo->height : vfmt.height;

    const int right  = std::min<int>(left + vfmt.width + 1, geo->x + geo->width);
    const int bottom = std::min<int>(top + vfmt.height + 1, geo->y + geo->height);

    vfmt.width   = (right - left) & (~1);
    vfmt.height  = (bottom - top) & (~1);
    vfmt.pix_fmt = x::from_xcb_pixmap_format(conn_, geo->depth);

    if (vfmt.pix_fmt == AV_PIX_FMT_NONE || vfmt.width <= 0 || vfmt.height <= 0) {
        LOG(INFO) << "[    LINUX-X] invalid pixel format";
        return -1;
    }
    bpp_ = av_get_padded_bits_per_pixel(av_pix_fmt_desc_get(vfmt.pix_fmt));

    // shared memory & ffmpeg buffer pool
    frame_size_ = av_image_get_buffer_size(vfmt.pix_fmt, vfmt.width, vfmt.height, 1);

    xshm_pool_ = av_buffer_pool_init2(frame_size_, conn_, xshm_alloc, nullptr);
    if (!xshm_pool_) {
        LOG(ERROR) << fmt::format("[    LINUX-X] failed to init buffer pool");
        return -1;
    }

    // xcb_xfixes
    const auto xfixes_version = xcb_xfixes_query_version_reply(
        conn_, xcb_xfixes_query_version(conn_, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION),
        nullptr);

    if (!xfixes_version) {
        LOG(ERROR) << fmt::format("[    LINUX-X] failed to query xfixes version");
        return -1;
    }
    ::free(xfixes_version);

    ready_ = true;

    LOG(INFO) << "[    LINUX-X] " << av::to_string(vfmt);
    return 0;
}

int XshmCapturer::start()
{
    if (running_ || !ready_) {
        LOG(WARNING) << "not ready or already running";
        return -1;
    }

    running_ = true;
    thread_  = std::jthread([this] {
        probe::thread::set_name("LINUX-XSHM");

        av::frame frame{};
        while (running_) {
            std::this_thread::sleep_for(30ms);

            frame.unref();

            auto buf = av_buffer_pool_get(xshm_pool_);
            if (!buf) {
                running_ = false;
                continue;
            }

            const auto xseg = static_cast<xcb_shm_seg_t>(
                reinterpret_cast<uintptr_t>(av_buffer_pool_buffer_get_opaque(buf)));
            const auto req = ::xcb_shm_get_image_unchecked(conn_, wid_, left, top, vfmt.width, vfmt.height,
                                                            ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, xseg, 0);
            const auto img = xcb_shm_get_image_reply(conn_, req, nullptr);
            defer(free(img));
            ::xcb_flush(conn_);

            if (!img) {
                LOG(ERROR) << "cannot get the image data";
                av_buffer_unref(&buf);

                running_ = false;
                continue;
            }

            frame->width       = vfmt.width;
            frame->height      = vfmt.height;
            frame->format      = vfmt.pix_fmt;
            frame->pts         = av::clock::ns().count();
            frame->linesize[0] = av_image_get_linesize(vfmt.pix_fmt, vfmt.width, 0);
            frame->buf[0]      = buf;
            frame->data[0]     = buf->data;

            if (draw_cursor && bpp_ >= 24) xfixes_draw_cursor(frame);

            DLOG(INFO) << fmt::format("[V] pts = {:>14d}, size = {:>4d}x{:>4d}, ts = {:.3%T}", frame->pts,
                                       frame->width, frame->height, std::chrono::nanoseconds{ frame->pts });
            onarrived(frame, AVMEDIA_TYPE_VIDEO);
        }
    });

    return 0;
}

void XshmCapturer::stop()
{
    running_ = false;
    if (thread_.joinable()) thread_.join();

    if (ready_) {
        ready_ = false;

        ::av_buffer_pool_uninit(&xshm_pool_);
        ::xcb_disconnect(conn_);
    }

    DLOG(INFO) << "[    LINUX-X] STOPPED";
}

XshmCapturer::~XshmCapturer()
{
    stop();
    DLOG(INFO) << "[    LINUX-X] ~";
}

bool XshmCapturer::has(AVMediaType mt) const { return mt == AVMEDIA_TYPE_VIDEO; }

#endif