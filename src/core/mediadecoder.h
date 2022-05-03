#ifndef CAPTURER_MEDIA_DECODER
#define CAPTURER_MEDIA_DECODER

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
#include <QThread>
#include <QImage>
#include <mutex>
#include "ringbuffer.h"
#include "utils.h"
#include "logging.h"

class MediaDecoder : public QObject {
	Q_OBJECT

public:
	~MediaDecoder()
	{
		close();
	}

	bool open(const std::string& name, const std::string& format, const std::map<std::string, std::string>& options)
	{
		// format context
		fmt_ctx_ = avformat_alloc_context();
		if (!fmt_ctx_) {
			return false;
		}

		avdevice_register_all();

		// input format
		AVInputFormat * input_fmt = nullptr;
		if (!format.empty()) {
			input_fmt = av_find_input_format(format.c_str());
			if (!input_fmt) {
				LOG(ERROR) << "av_find_input_format";
				return false;
			}
		}

		// options
		AVDictionary* input_options = nullptr;
		defer(av_dict_free(&input_options));
		for (const auto& [key, value] : options) {
			av_dict_set(&input_options, key.c_str(), value.c_str(), 0);
		}

		// open input
		if (avformat_open_input(&fmt_ctx_, name.c_str(), input_fmt, &input_options) < 0) {
			LOG(ERROR) << "avformat_open_input";
			return false;
		}

		if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
			LOG(ERROR) << "avformat_find_stream_info";
			return false;
		}

		av_dump_format(fmt_ctx_, 0, name.c_str(), 0);

		// find video stream
		if ((video_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0)) < 0) {
			LOG(ERROR) << "av_find_best_stream";
			return false;
		}

		// decoder
		decoder_ = avcodec_find_decoder(fmt_ctx_->streams[video_stream_index_]->codecpar->codec_id);
		if (!decoder_) {
			LOG(ERROR) << "avcodec_find_decoder";
			return false;
		}

		// decoder context
		decoder_ctx_ = avcodec_alloc_context3(decoder_);
		if (!decoder_ctx_) {
			LOG(ERROR) << "avcodec_alloc_context3";
			return false;
		}

		if (avcodec_parameters_to_context(decoder_ctx_, fmt_ctx_->streams[video_stream_index_]->codecpar) < 0) {
			LOG(ERROR) << "avcodec_parameters_to_context";
			return false;
		}

		// open codec
		AVDictionary* decoder_options = nullptr;
		defer(av_dict_free(&decoder_options));
		av_dict_set(&decoder_options, "threads", "auto", 0);
		if (avcodec_open2(decoder_ctx_, decoder_, &decoder_options) < 0) {
			LOG(ERROR) << "avcodec_open2";
			return false;
		}

		// prepare 
		packet_ = av_packet_alloc();
		if (!packet_) {
			LOG(ERROR) << "av_packet_alloc";
			return false;
		}

		frame_ = av_frame_alloc();
		if (!frame_) {
			LOG(ERROR) << "av_frame_alloc";
			return false;
		}

		sws_ctx_ = sws_getContext(
			decoder_ctx_->width, decoder_ctx_->height, decoder_ctx_->pix_fmt,
			decoder_ctx_->width, decoder_ctx_->height, AV_PIX_FMT_RGB24,
			SWS_BICUBIC, nullptr, nullptr, nullptr
		);
		if (!sws_ctx_) {
			LOG(ERROR) << "sws_getContext";
			return false;
		}

		rgb_frame_ = av_frame_alloc();
		if (!rgb_frame_) {
			LOG(ERROR) << "av_frame_alloc";
			return false;
		}
		if (av_image_alloc(rgb_frame_->data, rgb_frame_->linesize,
			decoder_ctx_->width, decoder_ctx_->height,
			AV_PIX_FMT_RGB24, 16) < 0) {
			LOG(ERROR) << "av_image_alloc";
			return false;
		}

		opened(true);
		LOG(INFO) << name << "is opened";
		return true;
	}

	bool running() { std::lock_guard<std::mutex> lock(mtx_); return running_; }
	bool paused() { std::lock_guard<std::mutex> lock(mtx_); return paused_; }

	void process()
	{
		if (!opened()) return;

		running(true);

		while (running()) {
			if (paused()) {
				QThread::msleep(40);
				continue;
			}

			int ret = av_read_frame(fmt_ctx_, packet_);
			if (ret < 0) {
				running(false);
				return;
			}

			ret = avcodec_send_packet(decoder_ctx_, packet_);
			while (ret >= 0) {
				ret = avcodec_receive_frame(decoder_ctx_, frame_);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				}
				else if (ret < 0) {
					running(false);
					return;
				}

				// decoded frame@{
				// convert the format to rgb888
				sws_scale(
					sws_ctx_,
					static_cast<const uint8_t* const*>(frame_->data), frame_->linesize,
					0, frame_->height,
					rgb_frame_->data, rgb_frame_->linesize
				);

				rgb_frame_->height = frame_->height;
				rgb_frame_->width = frame_->width;
				rgb_frame_->format = AV_PIX_FMT_RGB24;
				first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;
				rgb_frame_->pts = av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, decoder_ctx_->time_base);


				buffer_.push(QImage(
					static_cast<const uchar*>(rgb_frame_->data[0]),
					frame_->width, frame_->height,
					QImage::Format_RGB888
				).copy());
				
				emit received();
				// @}

				av_frame_unref(frame_);
			}
			av_packet_unref(packet_);
		}

		close();
	}

	int width() { return decoder_ctx_ ? decoder_ctx_->width : 480; }
	int height() { return decoder_ctx_ ? decoder_ctx_->height : 360; }

signals:
	void received();

public slots:
	void pause() { std::lock_guard<std::mutex> lock(mtx_); paused_ = true; }
	void resume() { std::lock_guard<std::mutex> lock(mtx_); paused_ = false; }

	void stop() { std::lock_guard<std::mutex> lock(mtx_); running_ = false; }

	QImage frame()
	{
		return buffer_.pop();
	}

private:
	void close()
	{
		running(false);

		av_packet_free(&packet_);
		av_frame_free(&frame_);
		av_frame_free(&rgb_frame_);

		avcodec_free_context(&decoder_ctx_);
		avformat_close_input(&fmt_ctx_);
	}

	void running(bool v) { std::lock_guard<std::mutex> lock(mtx_); running_ = true; }
	bool opened() { std::lock_guard<std::mutex> lock(mtx_); return opened_; }
	void opened(bool v) { std::lock_guard<std::mutex> lock(mtx_); opened_ = v; }

	bool running_{ false };
	bool paused_{ false };
	bool opened_{ false };

	AVFormatContext* fmt_ctx_{ nullptr };
	int video_stream_index_{ -1 };

	AVCodecContext* decoder_ctx_{ nullptr };
	AVCodec* decoder_{ nullptr };

	AVPacket* packet_{ nullptr };
	AVFrame* frame_{ nullptr };
	AVFrame* rgb_frame_{ nullptr };

	SwsContext* sws_ctx_{ nullptr };

	int64_t first_pts_{ AV_NOPTS_VALUE };
	std::mutex mtx_;

	RingBuffer<QImage, 25> buffer_;
};

#endif // !CAPTURER_MEDIA_DECODER
