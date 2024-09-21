#ifndef CAPTURER_FFMPEG_WRAPPER_H
#define CAPTURER_FFMPEG_WRAPPER_H

#include <utility>

extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

namespace av
{
    struct take_ownership_t
    {};

    inline constexpr take_ownership_t take_ownership{};
} // namespace av

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

namespace av
{
    template<typename T> struct buffer
    {
        buffer(std::nullptr_t = nullptr) noexcept {}

        buffer(AVBufferRef *ptr) noexcept
            : ptr_(av_buffer_ref(ptr))
        {}

        buffer(AVBufferRef *ptr, take_ownership_t) noexcept
            : ptr_(ptr)
        {}

        buffer(const buffer& other) noexcept
            : ptr_(av_buffer_ref(other.ptr_))
        {}

        buffer(buffer&& other) noexcept
            : ptr_(std::exchange(other.ptr_, {}))
        {}

        ~buffer() { release_ref(); }

        buffer& operator=(const buffer& other) noexcept
        {
            if (ptr_ != other.ptr_) {
                release_ref();
                ptr_ = av_buffer_ref(other.ptr_);
            }
            return *this;
        }

        buffer& operator=(buffer&& other) noexcept
        {
            if (this != &other) {
                release_ref();
                ptr_ = std::exchange(other.ptr_, {});
            }
            return *this;
        }

        buffer& operator=(AVBufferRef *ptr)
        {
            if (ptr_ != ptr) {
                release_ref();
                ptr_ = av_buffer_ref(ptr);
            }
            return *this;
        }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        auto operator->() const noexcept { return ptr_; }

        AVBufferRef& operator*() const noexcept { return *ptr_; }

        void attach(AVBufferRef *ptr) noexcept
        {
            if (ptr_ != ptr) {
                release_ref();
                ptr_ = ptr;
            }
        }

        AVBufferRef *detach() noexcept { return std::exchange(ptr_, {}); }

        [[nodiscard]] AVBufferRef *get() const noexcept { return ptr_; }

        T *data() const noexcept { return ptr_ ? reinterpret_cast<T *>(ptr_->data) : nullptr; }

    private:
        void release_ref() noexcept
        {
            if (ptr_) {
                av_buffer_unref(&ptr_);
            }
        }

        AVBufferRef *ptr_{};
    };
} // namespace av

#endif //! CAPTURER_FFMPEG_WRAPPER_H