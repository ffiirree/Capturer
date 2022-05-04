#ifndef CAPTURER_MEDIA_ENCODER_H
#define CAPTURER_MEDIA_ENCODER_H

extern "C" {
#include <libavformat\avformat.h>
#include <libavcodec\avcodec.h>
#include <libavdevice\avdevice.h>
#include <libswscale\swscale.h>
#include <libswresample\swresample.h>
#include <libavutil\avassert.h>
#include <libavutil\channel_layout.h>
#include <libavutil\opt.h>
#include <libavutil\mathematics.h>
#include <libavutil\timestamp.h>
#include <libavutil\error.h>
#include <libavcodec\adts_parser.h>
#include <libavutil\time.h>
#include <libavfilter\avfilter.h>
#include <libavfilter\buffersink.h>
#include <libavfilter\buffersrc.h>
#include <libavutil\imgutils.h>
#include <libavutil\samplefmt.h>
#include <libavutil\log.h>
}
#include <QImage>
#include <mutex>
#include "ringbuffer.h"
#include "utils.h"
#include "mediadecoder.h"
#include "logging.h"

class MediaEncoder : public QObject {
	Q_OBJECT

public:
	explicit MediaEncoder(MediaDecoder* decoder) : decoder_(decoder) {}

	~MediaEncoder()
	{
		close();
	}

	bool open(const std::string& filename, const std::string& codec_name, AVPixelFormat pix_fmt, AVRational framerate = { 25, 1 }, const std::map<std::string, std::string>& options = {})
	{
		pix_fmt_ = pix_fmt;

		// MediaDecoder
		if (!decoder_) {
			LOG(ERROR) << "decoder is null";
			return false;
		}

		// format context
		if (avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, filename.c_str()) < 0) {
			LOG(ERROR) << "avformat_alloc_output_context2";
			return false;
		}

		// new stream
		AVStream* stream = avformat_new_stream(fmt_ctx_, nullptr);
		if (!stream) {
			LOG(ERROR) << "avformat_new_stream";
			return false;
		}
		video_stream_index_ = stream->index;

		encoder_ = avcodec_find_encoder_by_name(codec_name.c_str());
		if (!encoder_) {
			LOG(ERROR) << "avcodec_find_encoder_by_name";
			return false;
		}

		encoder_ctx_ = avcodec_alloc_context3(encoder_);
		if (!encoder_ctx_) {
			LOG(ERROR) << "avcodec_alloc_context3";
			return false;
		}

		AVDictionary* encoder_options = nullptr;
		defer(av_dict_free(&encoder_options));
		for (const auto& [key, value] : options) {
			av_dict_set(&encoder_options, key.c_str(), value.c_str(), 0);
		
		}
		if (codec_name == "libx264" || codec_name == "x265") {
			av_dict_set(&encoder_options, "preset", "ultrafast", AV_DICT_DONT_OVERWRITE);
			av_dict_set(&encoder_options, "tune", "zerolatency", AV_DICT_DONT_OVERWRITE);
			av_dict_set(&encoder_options, "crf", "23", AV_DICT_DONT_OVERWRITE);
			av_dict_set(&encoder_options, "threads", "auto", AV_DICT_DONT_OVERWRITE);
		}

		encoder_ctx_->height = decoder_->height();
		encoder_ctx_->width = decoder_->width();
		encoder_ctx_->pix_fmt = pix_fmt_;
		encoder_ctx_->sample_aspect_ratio = decoder_->sar();
		
		// TODO: Timebase: this is the fundamental unit of time (in seconds) in terms of which frame
        // timestamps are represented. For fixed-fps content, timebase should be 1/framerate
        // and timestamp increments should be identical to 1.
		encoder_ctx_->time_base = decoder_->timebase();
		fmt_ctx_->streams[video_stream_index_]->time_base = decoder_->timebase();
		encoder_ctx_->framerate = framerate;

		if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
			encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}

		if (avcodec_open2(encoder_ctx_, encoder_, &encoder_options) < 0) {
			LOG(ERROR) << "avcodec_open2";
			return false;
		}

		if (avcodec_parameters_from_context(fmt_ctx_->streams[video_stream_index_]->codecpar, encoder_ctx_) < 0) {
			LOG(ERROR) << "avcodec_parameters_from_context";
			return false;
		}

		if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
			if (avio_open(&fmt_ctx_->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
				LOG(ERROR) << "avio_open";
				return false;
			}
		}

		if (avformat_write_header(fmt_ctx_, nullptr) < 0) {
			LOG(ERROR) << "avformat_write_header";
			return false;
		}

		// prepare 
		packet_ = av_packet_alloc();
		if (!packet_) {
			LOG(ERROR) << "av_packet_alloc";
			return false;
		}

		decoded_frame_ = av_frame_alloc();
		if (!decoded_frame_) {
			LOG(ERROR) << "av_frame_alloc";
			return false;
		}

		if (av_image_alloc(decoded_frame_->data, decoded_frame_->linesize, encoder_ctx_->width, encoder_ctx_->height, pix_fmt_, 16) < 0) {
			LOG(ERROR) << "av_image_alloc";
			return false;
		}

		opened(true);
		
		av_dump_format(fmt_ctx_, 0, filename.c_str(), 1);

		LOG(INFO) << "[ENCODER] " << filename << " is opened";
		return true;
	}

	void process()
	{
		LOG(INFO) << "[ENCODER] STARTED@" << QThread::currentThreadId();
		LOG(INFO) << "[ENCODER] FRAMERATE = " << encoder_ctx_->framerate.num;

		if (!opened() || !decoder_ || !decoder_->opened()) return;

		emit started();

		first_pts_ = AV_NOPTS_VALUE;
		while (decoder_ && decoder_->running()) {
			if (decoder_->paused()) {
				QThread::msleep(40);
				continue;
			}

			if (decoder_->read(decoded_frame_) < 0) {
				QThread::msleep(40);
				continue;
			}

			int64_t duration = av_q2d(av_div_q(av_inv_q(encoder_ctx_->framerate), encoder_ctx_->time_base));
			first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;
			decoded_frame_->pts = av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, encoder_ctx_->time_base);
			int64_t delta = std::max(0ll, encoder_ctx_->frame_number * duration - (av_gettime_relative() - first_pts_));
			int64_t next_pts = av_gettime_relative() + std::min(static_cast<int64_t>(duration * 1.1), delta);

			int ret = avcodec_send_frame(encoder_ctx_, decoded_frame_);
			while (ret >= 0) {
				av_packet_unref(packet_);
				ret = avcodec_receive_packet(encoder_ctx_, packet_);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				}
				else if (ret < 0) {
					LOG(ERROR) << "encoding failure";
					return;
				}

				packet_->stream_index = video_stream_index_;
				packet_->duration = duration;

				ret = av_interleaved_write_frame(fmt_ctx_, packet_);
				if (ret != 0) {
					LOG(ERROR) << "av_interleaved_write_frame";
					return;
				}
			}

			QThread::msleep(std::max(0ll, (next_pts - av_gettime_relative()) / (AV_TIME_BASE / 1000)));
		}

		LOG(INFO) << "[ENCODER]" << "encoded frames: " << encoder_ctx_->frame_number;

		close();
		emit stopped();
		LOG(INFO) << "[ENCODER] EXITED ";
	}

	int width() { return encoder_ctx_ ? encoder_ctx_->width : 0; }
	int height() { return encoder_ctx_ ? encoder_ctx_->height : 0; }
	bool opened() { std::lock_guard<std::mutex> lock(mtx_); return opened_; }

signals:
	void started();
	void stopped();

private:
	void close()
	{
		opened(false);

		first_pts_ = AV_NOPTS_VALUE;

		if (av_write_trailer(fmt_ctx_) != 0) {
			LOG(ERROR) << "av_write_trailer";
		}

		if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
			if (avio_close(fmt_ctx_->pb) < 0) {
				LOG(ERROR) << "avio_close";
			}
		}

		av_packet_free(&packet_);

		avcodec_free_context(&encoder_ctx_);
		avformat_free_context(fmt_ctx_);

		LOG(INFO) << "[ENCODER] CLOSED";
	}
	
	void opened(bool v) { std::lock_guard<std::mutex> lock(mtx_); opened_ = v; }

	bool opened_{ false };

	MediaDecoder *decoder_{ nullptr };
	AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };

	AVFormatContext* fmt_ctx_{ nullptr };
	int video_stream_index_{ -1 };

	AVCodecContext* encoder_ctx_{ nullptr };
	AVCodec* encoder_{ nullptr };

	AVPacket* packet_{ nullptr };
	AVFrame* decoded_frame_{ nullptr };

	int64_t first_pts_{ AV_NOPTS_VALUE };
	std::mutex mtx_;
};
#endif // !CAPTURER_MEDIA_ENCODER_H
