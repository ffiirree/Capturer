#ifndef CAPTURER_FFMPEG_WRAPPER_H
#define CAPTURER_FFMPEG_WRAPPER_H

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

    template<typename T> struct ptr
    {
        using value_type      = T;
        using pointer         = T *;
        using const_pointer   = const T *;
        using reference       = T&;
        using const_reference = const T&;

        explicit ptr(std::nullptr_t = nullptr) { alloc(); }

        explicit ptr(const T *ptr)
        {
            alloc();
            ffmpeg::ref(ptr_, ptr);
        }

        ptr(const ptr& other)
        {
            alloc();
            ffmpeg::ref(ptr_, other.ptr_);
        }

        ptr(ptr&& other) noexcept
        {
            ptr_       = other.ptr_;
            other.ptr_ = nullptr;
        }

        ptr& operator=(const T *other)
        {
            unref();
            ffmpeg::ref(ptr_, other);
            return *this;
        }

        ptr& operator=(const ptr& other)
        {
            unref();
            ffmpeg::ref(ptr_, other.ptr_);
            return *this;
        }

        ptr& operator=(ptr&& other) noexcept
        {
            if (this != &other) {
                ptr_       = other.ptr_;
                other.ptr_ = nullptr;
            }

            return *this;
        }

        ~ptr() { release(); }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        auto get() const noexcept { return ptr_; }

        auto put()
        {
            unref();
            return ptr_;
        }

        auto operator->() const noexcept { return ptr_; }

        void ref(const T *other)
        {
            unref();
            ffmpeg::ref(ptr_, other);
        }

        void move_ref(const T *other)
        {
            unref();
            ffmpeg::move_ref(ptr_, other);
        }

        void unref() { ffmpeg::unref(ptr_); }

        void reset()
        {
            if (ptr_) release();
            alloc();
        }

    private:
        void alloc() { ffmpeg::alloc(&ptr_); }

        void release() { ffmpeg::free(&ptr_); }

        T *ptr_{ nullptr };
    };

    using frame  = ptr<AVFrame>;
    using packet = ptr<AVPacket>;
} // namespace av
#endif //! CAPTURER_FFMPEG_WRAPPER_H