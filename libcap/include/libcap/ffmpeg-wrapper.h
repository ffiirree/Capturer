#ifndef CAPTURER_FFMPEG_WRAPPER_H
#define CAPTURER_FFMPEG_WRAPPER_H

#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

namespace av
{
    namespace ffmpeg
    {
        inline int ref(AVPacket *dst, const AVPacket *src) { return av_packet_ref(dst, src); }

        inline int ref(AVFrame *dst, const AVFrame *src) { return av_frame_ref(dst, src); }

        inline void unref(AVPacket *packet) { av_packet_unref(packet); }

        inline void unref(AVFrame *frame) { av_frame_unref(frame); }

        inline void move_ref(AVPacket *dst, AVPacket *src) { av_packet_move_ref(dst, src); }

        inline void move_ref(AVFrame *dst, AVFrame *src) { av_frame_move_ref(dst, src); }

        inline void alloc(AVPacket **packet) { *packet = av_packet_alloc(); }

        inline void alloc(AVFrame **frame) { *frame = av_frame_alloc(); }

        inline void free(AVPacket **packet) { av_packet_free(packet); }

        inline void free(AVFrame **frame) { av_frame_free(frame); }

        inline void free(AVCodecContext **avctx) { avcodec_free_context(avctx); }
    } // namespace ffmpeg

    template<typename T>
    requires std::same_as<T, AVFrame> || std::same_as<T, AVPacket>
    struct ptr
    {
        using value_type      = T;
        using pointer         = T *;
        using const_pointer   = const T *;
        using reference       = T&;
        using const_reference = const T&;

        ptr() { ffmpeg::alloc(&ptr_); }

        ptr(std::nullptr_t) noexcept {}

        // ref
        ptr(const ptr& other)
        {
            if (other.ptr_) {
                ffmpeg::alloc(&ptr_);
                ffmpeg::ref(ptr_, other.ptr_);
            }
        }

        ptr(ptr&& other) noexcept
        {
            if (other.ptr_) {
                ffmpeg::alloc(&ptr_);
                ffmpeg::move_ref(ptr_, other.ptr_);
            }
        }

        ~ptr() { release(); }

        ptr& operator=(const ptr& other)
        {
            if (this != &other) {
                if (other.ptr_) {
                    ptr_ ? ffmpeg::unref(ptr_) : ffmpeg::alloc(&ptr_);
                    ffmpeg::ref(ptr_, other.ptr_);
                }
                // = nullptr
                else {
                    release();
                }
            }

            return *this;
        }

        ptr& operator=(ptr&& other) noexcept
        {
            if (this != &other) {
                if (other.ptr_) {
                    ptr_ ? ffmpeg::unref(ptr_) : ffmpeg::alloc(&ptr_);
                    ffmpeg::move_ref(ptr_, other.ptr_);
                }
                // = nullptr
                else {
                    release();
                }
            }
            return *this;
        }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        auto get() const noexcept { return ptr_; }

        auto put()
        {
            ptr_ ? ffmpeg::unref(ptr_) : ffmpeg::alloc(&ptr_);
            return ptr_;
        }

        auto operator->() const noexcept { return ptr_; }

        void unref()
        {
            if (ptr_) {
                ffmpeg::unref(ptr_);
            }
        }

    private:
        void release()
        {
            if (ptr_) {
                ffmpeg::free(&ptr_);
                ptr_ = nullptr;
            }
        }

        value_type *ptr_{};
    };

    using frame  = ptr<AVFrame>;
    using packet = ptr<AVPacket>;
} // namespace av
#endif //! CAPTURER_FFMPEG_WRAPPER_H