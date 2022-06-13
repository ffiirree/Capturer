#include <chrono>
#include "dispatcher.h"
#include "logging.h"
#include "fmt/format.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
using namespace std::chrono_literals;

int Dispatcher::create_filter_for_video_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] decoder is not ready";
        return -1;
    }
    LOG(INFO) << "[DISPATCHER] " << "create filter for video buffersrc: " << decoder->format_str(AVMEDIA_TYPE_VIDEO);

    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    if (!buffersrc) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersrc, "v_src", decoder->format_str(AVMEDIA_TYPE_VIDEO).c_str(), nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    return 0;
}

int Dispatcher::create_filter_for_video_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    if (!buffersink) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersink, "v_sink", nullptr, nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    enum AVPixelFormat pix_fmt = ((AVPixelFormat)encoder->format(AVMEDIA_TYPE_VIDEO) == AV_PIX_FMT_NONE) ? AV_PIX_FMT_YUV420P : (AVPixelFormat)encoder->format(AVMEDIA_TYPE_VIDEO);
    enum AVPixelFormat pix_fmts[] = { pix_fmt, AV_PIX_FMT_NONE };
    if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "av_opt_set_int_list";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] " << "create filter for video buffersink";

    return 0;
}

int Dispatcher::create_filter_for_audio_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] decoder is not ready";
        return -1;
    }
    LOG(INFO) << "[DISPATCHER] " << "create filter for audio buffersrc: " << decoder->format_str(AVMEDIA_TYPE_AUDIO);

    const AVFilter* buffersrc = avfilter_get_by_name("abuffer");
    if (!buffersrc) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersrc, "a_src", decoder->format_str(AVMEDIA_TYPE_AUDIO).c_str(), nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    has_audio_in_ = true;
    return 0;
}

int Dispatcher::create_filter_for_audio_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffersink) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, abuffersink, "a_sink", nullptr, nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    enum AVSampleFormat encoder_fmt = (AVSampleFormat)encoder->format(AVMEDIA_TYPE_AUDIO) ;
    enum AVSampleFormat sink_fmt = (encoder_fmt == AV_SAMPLE_FMT_NONE) ? AV_SAMPLE_FMT_FLTP : encoder_fmt;
    enum AVSampleFormat sink_fmts[] = { sink_fmt, AV_SAMPLE_FMT_NONE };
    if (av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "av_opt_set_int_list";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] " << "create filter for audio buffersink";

    return 0;
}

int Dispatcher::create_filter_graph(const std::string_view& filters)
{
    filters_ = filters;

    if (filter_graph_) {
        avfilter_graph_free(&filter_graph_);
    }

    // filters
    if (!(filter_graph_ = avfilter_graph_alloc())) {
        LOG(ERROR) << "avfilter_graph_alloc";
        return -1;
    }

    //if ((decoders_.size() > 1 || encoders_.size() > 1) && filters_.length() == 0) {
    //    LOG(ERROR) << "parameters error";
    //    return -1;
    //}

    // buffersrc
    for (auto& coder : decoders_) {
        if (coder.producer->has(AVMEDIA_TYPE_VIDEO) &&
            create_filter_for_video_input(coder.producer, &coder.video_src_ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_input";
            return -1;
        }

        if (coder.producer->has(AVMEDIA_TYPE_AUDIO) &&
            create_filter_for_audio_input(coder.producer, &coder.audio_src_ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_input";
            return -1;
        }
    }

    // buffersink
    for (auto& coder : encoders_) {
        if (coder.consumer->accepts(AVMEDIA_TYPE_VIDEO) &&
            create_filter_for_video_output(coder.consumer, &coder.video_sink_ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_output";
            return -1;
        }

        if (!has_audio_in_) {
            coder.consumer->enable(AVMEDIA_TYPE_AUDIO, false);
        }
        else if (coder.consumer->accepts(AVMEDIA_TYPE_AUDIO) &&
                 create_filter_for_audio_output(coder.consumer, &coder.audio_sink_ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_output";
            return -1;
        }
    }

    if (filters_.length() > 0) {
        AVFilterInOut* inputs = nullptr;
        AVFilterInOut* outputs = nullptr;
        defer(avfilter_inout_free(&inputs));
        defer(avfilter_inout_free(&outputs));

        LOG(FATAL) << "not support";

        if (avfilter_graph_parse2(filter_graph_, filters_.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "avfilter_graph_parse2";
            return -1;
        }

        auto index = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, index++) {
            if (decoders_[index].video_src_ctx && avfilter_link(decoders_[index].video_src_ctx, 0, ptr->filter_ctx, ptr->pad_idx) != 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }

            ptr = ptr->next;
            if (decoders_[index].audio_src_ctx && avfilter_link(decoders_[index].audio_src_ctx, 0, ptr->filter_ctx, ptr->pad_idx) != 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }
        }
        index = 0;
        for (auto ptr = outputs; ptr; ptr = ptr->next, index++) {
            if (encoders_[index].video_sink_ctx && avfilter_link(ptr->filter_ctx, ptr->pad_idx, encoders_[index].video_sink_ctx, 0) != 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }

            ptr = ptr->next;
            if (encoders_[index].audio_sink_ctx && avfilter_link(ptr->filter_ctx, ptr->pad_idx, encoders_[index].audio_sink_ctx, 0) != 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }
        }
    }
    else {
        if (avfilter_link(decoders_[0].video_src_ctx, 0, encoders_[0].video_sink_ctx, 0) != 0) {
            LOG(ERROR) << "avfilter_link";
            return -1;
        }

        if (has_audio_in_ && encoders_[0].audio_sink_ctx && avfilter_link(decoders_[1].audio_src_ctx, 0, encoders_[0].audio_sink_ctx, 0) != 0) {
            LOG(ERROR) << "avfilter_link";
            return -1;
        }
    }

    if (avfilter_graph_config(filter_graph_, nullptr) < 0) {
        LOG(ERROR) << "avfilter_graph_config(filter_graph_, nullptr)";
        return -1;
    }

    ready_ = true;
    LOG(INFO) << "[ENCODER] " << "filter graph @{";
    LOG(INFO) << "\n" << avfilter_graph_dump(filter_graph_, nullptr);
    LOG(INFO) << "[ENCODER] " << "@}";
    return 0;
}

int Dispatcher::start()
{
    if (!ready_ || running_) {
        LOG(INFO) << "[DISPATCHER] already running or not ready!";
        return -1;
    }

    for (auto& coder : decoders_) {
        if (!coder.producer->ready()) {
            LOG(ERROR) << "deocoder not ready!";
            return -1;
        }
    }

    for (auto& coder : encoders_) {
        if (!coder.consumer->ready()) {
            LOG(ERROR) << "encoder not ready!";
            return -1;
        }
    }

    for (auto& coder : decoders_) {
        coder.producer->run();
    }

    for (auto& coder : encoders_) {
        coder.consumer->run();
    }

    running_ = true;
    dispatch_thread_ = std::thread([this]() { dispatch_thread_f(); });

    return 0;
}

int Dispatcher::dispatch_thread_f()
{
    AVFrame* frame_ = av_frame_alloc();
    defer(av_frame_free(&frame_));
    AVFrame* filtered_frame = av_frame_alloc();
    defer(av_frame_free(&filtered_frame));

    LOG(INFO) << "[DISPATCHER@" << std::this_thread::get_id() << "] STARTED";
    defer(LOG(INFO) << "[DISPATCHER@" << std::this_thread::get_id() << "] EXITED");

    int ret = 0;
    bool need_dispatch = false;
    while (running_) {
        for (auto& decoder : decoders_) {
            if (decoder.video_src_ctx && decoder.producer->produce(frame_, AVMEDIA_TYPE_VIDEO) >= 0) {

                ret = av_buffersrc_add_frame_flags(
                    decoder.video_src_ctx,
                    (!frame_->width && !frame_->height) ? nullptr : frame_,
                    AV_BUFFERSRC_FLAG_PUSH
                );
                if (ret < 0) {
                    LOG(ERROR) << "av_buffersrc_add_frame_flags";
                    running_ = false;
                    break;
                }

                need_dispatch = true;
            }

            if (decoder.audio_src_ctx && decoder.producer->produce(frame_, AVMEDIA_TYPE_AUDIO) >= 0) {

                ret = av_buffersrc_add_frame_flags(
                    decoder.audio_src_ctx,
                    (!frame_->linesize[0]) ? nullptr : frame_,
                    AV_BUFFERSRC_FLAG_PUSH
                );
                if (ret < 0) {
                    LOG(ERROR) << "av_buffersrc_add_frame_flags";
                    running_ = false;
                    break;
                }

                need_dispatch = true;
            }
        }

        if (!need_dispatch) {
            std::this_thread::sleep_for(20ms);
            need_dispatch = true;
            LOG(INFO) << "[DISPATCHER] sleep 20ms";
            continue;
        }

        first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

        // pause @{
        if (paused_) {
            if (paused_pts_ != AV_NOPTS_VALUE) {
                offset_pts_ += av_gettime_relative() - paused_pts_;
            }

            paused_pts_ = av_gettime_relative();

            std::this_thread::sleep_for(20ms);
            continue;
        }

        if (paused_pts_ != AV_NOPTS_VALUE) {
            offset_pts_ += av_gettime_relative() - paused_pts_;
            paused_pts_ = AV_NOPTS_VALUE;
        }
        // @}

        for (auto& encoder : encoders_) {
            while (ret >= 0 && !(encoder.eof & 0x01)) {
                av_frame_unref(filtered_frame);
                ret = av_buffersink_get_frame_flags(encoder.video_sink_ctx, filtered_frame, AV_BUFFERSINK_FLAG_NO_REQUEST);

                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[DISPATCHER] VIDEO EOF";
                    encoder.eof |= 0x01;
                    // through and send null packet
                    av_frame_unref(filtered_frame);
                }
                else if (ret < 0) {
                    LOG(ERROR) << "av_buffersink_get_frame_flags()";
                    running_ = false;
                    break;
                }

                while (encoder.consumer->full(AVMEDIA_TYPE_VIDEO) && running_) {
                    std::this_thread::sleep_for(20ms);
                }
                ret = encoder.consumer->consume(filtered_frame, AVMEDIA_TYPE_VIDEO);
            }

            ret = 0;
            while (ret >= 0 && encoder.audio_sink_ctx && !(encoder.eof & 0x02)) {
                av_frame_unref(filtered_frame);
                ret = av_buffersink_get_frame_flags(encoder.audio_sink_ctx, filtered_frame, AV_BUFFERSINK_FLAG_NO_REQUEST);

                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[DISPATCHER] AUDIO EOF";
                    encoder.eof |= 0x02;
                    // through and send null packet
                    av_frame_unref(filtered_frame);
                }
                else if (ret < 0) {
                    LOG(ERROR) << "av_buffersink_get_frame_flags()";
                    running_ = false;
                    break;
                }

                while (encoder.consumer->full(AVMEDIA_TYPE_AUDIO) && running_) {
                    std::this_thread::sleep_for(20ms);
                }
                ret = encoder.consumer->consume(filtered_frame, AVMEDIA_TYPE_AUDIO);
            }
        }
    }

    return ret;
}

void Dispatcher::pause()
{
    for (auto& coder : decoders_) {
        coder.producer->pause();
    }

    for (auto& coder : encoders_) {
        coder.consumer->pause();
    }

    paused_ = true;
}

void Dispatcher::resume()
{
    paused_ = false;

    for (auto& coder : encoders_) {
        coder.consumer->resume();
    }

    for (auto& coder : decoders_) {
        coder.producer->time_offset(offset_pts_);
        coder.producer->resume();
    }
}

int Dispatcher::reset()
{
    paused_ = false;
    ready_ = false;

    for (auto& coder : decoders_) {
        coder.producer->reset();
    }

    for (auto& coder : encoders_) {
        // wait 0~3s
        for (int i = 0; i < 100; i++) {
            if (!coder.consumer->ready() || coder.consumer->eof()) break;

            std::this_thread::sleep_for(100ms);
        }
        coder.consumer->reset();
    }

    running_ = false;

    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }

    decoders_.clear();
    encoders_.clear();

    first_pts_ = AV_NOPTS_VALUE;
    paused_pts_ = AV_NOPTS_VALUE;
    offset_pts_ = 0;

    avfilter_graph_free(&filter_graph_);

    LOG(INFO) << "[DISPATCHER] RESETED";
    return 0;
}