#include "libcap/win-mfvc/mfvc-capturer.h"

#include <libcap/win-mfvc/win-mfvc.h>

#ifdef _WIN32

#include "logging.h"

#include <fmt/chrono.h>
#include <mfapi.h>
#include <probe/defer.h>

extern "C" {
#include <libavutil/imgutils.h>
}

class ReaderCallback : public winrt::implements<ReaderCallback, IMFSourceReaderCallback>
{
public:
    explicit ReaderCallback(const std::function<void(IMFSample *)>& callback)
        : callback_(callback)
    {}

    HRESULT STDMETHODCALLTYPE OnReadSample(HRESULT Status, DWORD StreamIndex, DWORD StreamFlags,
                                           LONGLONG Timestamp, IMFSample *Sample) override;

    HRESULT STDMETHODCALLTYPE OnFlush(DWORD StreamIndex) override;

    HRESULT STDMETHODCALLTYPE OnEvent(DWORD StreamIndex, IMFMediaEvent *Event) override;

private:
    std::function<void(IMFSample *)> callback_ = [](auto) {};
};

HRESULT ReaderCallback::OnReadSample(const HRESULT Status, DWORD StreamIndex, DWORD StreamFlags,
                                     LONGLONG Timestamp, IMFSample *Sample)
{
    if (FAILED(Status) || StreamFlags > 0 || !Sample) {
        callback_(nullptr);
        return S_FALSE;
    }

    callback_(Sample);

    return S_OK;
}

HRESULT ReaderCallback::OnFlush(DWORD) { return S_OK; }

HRESULT ReaderCallback::OnEvent(DWORD, IMFMediaEvent *) { return S_OK; }

int MFCameraCapturer::open(const std::string& device_id, std::map<std::string, std::string> options)
{
    try {
        winrt::com_ptr<IMFAttributes> attributes{};
        winrt::check_hresult(MFCreateAttributes(attributes.put(), 2));

        winrt::check_hresult(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                                 MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
        winrt::check_hresult(attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                                                   probe::util::to_utf16(device_id).c_str()));

        winrt::check_hresult(MFCreateDeviceSource(attributes.get(), source_.put()));

        const auto callback = winrt::make_self<ReaderCallback>([this](auto sample) { handler(sample); });
        winrt::check_hresult(MFCreateAttributes(attributes.put(), 1));
        winrt::check_hresult(attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback.get()));

        winrt::check_hresult(
            MFCreateSourceReaderFromMediaSource(source_.get(), attributes.get(), reader_.put()));

        // format
        winrt::com_ptr<IMFPresentationDescriptor> pd{};
        winrt::check_hresult(source_->CreatePresentationDescriptor(pd.put()));

        winrt::com_ptr<IMFStreamDescriptor> sd{};
        BOOL                                selected;
        winrt::check_hresult(pd->GetStreamDescriptorByIndex(0, &selected, sd.put()));

        winrt::com_ptr<IMFMediaTypeHandler> handler{};
        winrt::check_hresult(sd->GetMediaTypeHandler(handler.put()));

        winrt::com_ptr<IMFMediaType> type{};
        winrt::check_hresult(handler->GetCurrentMediaType(type.put()));

        GUID mffmt{};
        winrt::check_hresult(type->GetGUID(MF_MT_SUBTYPE, &mffmt));

        UINT32 compressed{};
        type->GetUINT32(MF_MT_COMPRESSED, &compressed);

        UINT32 sample_size{};
        winrt::check_hresult(type->GetUINT32(MF_MT_SAMPLE_SIZE, &sample_size));

        const auto [codec_id, pix_fmt] = mfvc::to_ffmpeg_format(mffmt);

        UINT32 fps_num{}, fps_dem{};
        winrt::check_hresult(MFGetAttributeRatio(type.get(), MF_MT_FRAME_RATE, &fps_num, &fps_dem));
        UINT32 w{}, h{};
        winrt::check_hresult(MFGetAttributeSize(type.get(), MF_MT_FRAME_SIZE, &w, &h));
        UINT32 sar_num{}, sar_den{};
        winrt::check_hresult(MFGetAttributeRatio(type.get(), MF_MT_PIXEL_ASPECT_RATIO, &sar_num, &sar_den));

        vfmt.pix_fmt             = pix_fmt;
        vfmt.width               = static_cast<int>(w);
        vfmt.height              = static_cast<int>(h);
        vfmt.framerate           = { static_cast<int>(fps_num), static_cast<int>(fps_dem) };
        vfmt.sample_aspect_ratio = { static_cast<int>(sar_num), static_cast<int>(sar_den) };

        // decoder
        auto codec = avcodec_find_decoder(codec_id);
        if (vcodec_ctx_ = avcodec_alloc_context3(codec); !vcodec_ctx_) {
            LOG(ERROR) << "[   WIN-MFVC] failed to alloc decoder context";
            return -1;
        }

        vcodec_ctx_->codec_type  = AVMEDIA_TYPE_VIDEO;
        vcodec_ctx_->codec_id    = codec_id;
        vcodec_ctx_->pix_fmt     = vfmt.pix_fmt;
        vcodec_ctx_->width       = vfmt.width;
        vcodec_ctx_->height      = vfmt.height;
        vcodec_ctx_->flags2     |= AV_CODEC_FLAG2_FAST;
        if (vfmt.pix_fmt != AV_PIX_FMT_NONE)
            vcodec_ctx_->frame_size = av_image_get_buffer_size(vfmt.pix_fmt, vfmt.width, vfmt.height, 1);

        if (codec_id == AV_CODEC_ID_RAWVIDEO) {
            vcodec_ctx_->codec_tag = avcodec_pix_fmt_to_codec_tag(vfmt.pix_fmt);
        }

        if (avcodec_open2(vcodec_ctx_, codec, nullptr) < 0) {
            LOG(ERROR) << "[   WIN-MFVC] can not open the decoder";
            return -1;
        }

        ready_ = true;

        // probe pixel format
        if (codec_id != AV_CODEC_ID_NONE && vcodec_ctx_->pix_fmt == AV_PIX_FMT_NONE) {
            onarrived = [this](const av::frame& frame, auto) {
                vfmt.pix_fmt = static_cast<AVPixelFormat>(frame->format);
            };

            start();
            for (int i = 0; i < 100 && vfmt.pix_fmt == AV_PIX_FMT_NONE; ++i) {
                std::this_thread::sleep_for(25ms);
            }
            stop();
            onarrived = [](auto, auto) {};

            if (vfmt.pix_fmt == AV_PIX_FMT_NONE) {
                ready_ = false;
                LOG(ERROR) << "[   WIN-MFVC] failed to probe the pixel format";
                return -1;
            }
        }
    }
    catch (const winrt::hresult_error& e) {
        LOG(ERROR) << "[   WIN-MFVC] " << probe::util::to_utf8(e.message().c_str());
        return -1;
    }

    ready_ = true;

    logi("[   WIN-MFVC] {} is opened, {}", device_id, av::to_string(vfmt));
    return 0;
}

int MFCameraCapturer::start()
{
    if (!ready_ || running_) {
        LOG(WARNING) << "[   WIN-MFVC] already running or not ready";
        return -1;
    }

    if (FAILED(reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr,
                                   nullptr))) {
        LOG(ERROR) << "[   WIN-MFVC] failed to start asynchronous mode";
        return -1;
    }

    return 0;
}

void MFCameraCapturer::handler(IMFSample *Sample)
{
    winrt::com_ptr<IMFMediaBuffer> buffer{};
    if (Sample && SUCCEEDED(Sample->GetBufferByIndex(0, buffer.put()))) {

        DWORD Length{};
        BYTE *data{};
        if (SUCCEEDED(buffer->Lock(&data, nullptr, &Length) && Length)) {
            av::packet pkt{};
            if (av_new_packet(pkt.put(), static_cast<int>(Length)) < 0) {
                loge(" [   WIN-MFVC]failed to allocate packet");
            }
            else {
                std::memcpy(pkt->data, data, Length);
                pkt->pts = av::clock::ns().count();
                //
                video_decode(pkt);

                logi("pts = {}, ts = {:.3%T}", pkt->pts, av::clock::ns());
            }
        }
        buffer->Unlock();
    }
    reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
}

int MFCameraCapturer::video_decode(const av::packet& pkt)
{
    auto ret = avcodec_send_packet(vcodec_ctx_, pkt.get());
    while (ret >= 0) {
        ret = avcodec_receive_frame(vcodec_ctx_, frame_.put());
        if (ret == AVERROR(EAGAIN)) {
            break;
        }
        else if (ret < 0) {
            running_ = false;
            loge("[V] DECODING ERROR, ABORTING");
            break;
        }

        onarrived(frame_, AVMEDIA_TYPE_VIDEO);
    }

    return ret;
}

void MFCameraCapturer::stop()
{
    if (source_) source_->Shutdown();
}

MFCameraCapturer::~MFCameraCapturer()
{
    stop();

    avcodec_free_context(&vcodec_ctx_);

    logi("[   WIN-MFVC] ~");
}

bool MFCameraCapturer::has(const AVMediaType type) const { return type == AVMEDIA_TYPE_VIDEO; }

bool MFCameraCapturer::is_realtime() const { return true; }

std::vector<av::vformat_t> MFCameraCapturer::video_formats() const { return { vfmt }; }

#endif