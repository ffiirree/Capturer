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

const int FRAME_BUFFER_SIZE = 15;

class MediaDecoder : public QObject {
	Q_OBJECT

public:
	explicit MediaDecoder() {}
	~MediaDecoder()
	{
		close();
	}

	bool open(const std::string& name, const std::string& format, AVPixelFormat pix_fmt, const std::map<std::string, std::string>& options)
	{
		pix_fmt_ = pix_fmt;

		// format context
		fmt_ctx_ = avformat_alloc_context();
		if (!fmt_ctx_) {
			return false;
		}

		avdevice_register_all();

		// input format
		AVInputFormat* input_fmt = nullptr;
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
			decoder_ctx_->width, decoder_ctx_->height, pix_fmt_,
			SWS_BICUBIC, nullptr, nullptr, nullptr
		);
		if (!sws_ctx_) {
			LOG(ERROR) << "sws_getContext";
			return false;
		}

		// buffer
		for (size_t i = 0; i < FRAME_BUFFER_SIZE; i++) {
			buffer_[i] = av_frame_alloc();
			if (!buffer_[i]) {
				LOG(ERROR) << "av_frame_alloc";
				return false;
			}

			if (av_image_alloc(buffer_[i]->data, buffer_[i]->linesize, decoder_ctx_->width, decoder_ctx_->height, pix_fmt_, 16) < 0) {
				LOG(ERROR) << "av_image_alloc";
				return false;
			}
			buffer_[i]->width = decoder_ctx_->width;
			buffer_[i]->height = decoder_ctx_->height;
			buffer_[i]->format = pix_fmt_;
		}

		opened(true);
		LOG(INFO) << "[DECODER]: " << name << " is opened";
		return true;
	}

	bool running() { std::lock_guard<std::mutex> lock(mtx_); return running_; }
	bool paused() { std::lock_guard<std::mutex> lock(mtx_); return paused_; }

	void process()
	{
		LOG(INFO) << "[DECODER] STARTED@" << QThread::currentThreadId();

		if (!opened()) return;

		running(true);
		emit started();

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
				// convert the format to AV_PIX_FMT_RGB24
				buffer_.push([this](AVFrame* buffer_frame) {
					sws_scale(
						sws_ctx_,
						static_cast<const uint8_t* const*>(frame_->data), frame_->linesize,
						0, frame_->height,
						buffer_frame->data, buffer_frame->linesize
					);

					buffer_frame->height = frame_->height;
					buffer_frame->width = frame_->width;
					buffer_frame->format = pix_fmt_;
					first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;
					buffer_frame->pts = av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, decoder_ctx_->time_base);
				});

				emit received();
				// @}

				av_frame_unref(frame_);
			}
			av_packet_unref(packet_);
		}

		LOG(INFO) << "[DECODER] " << "decoded frames: " << decoder_ctx_->frame_number;

		close();
		emit stopped();
		LOG(INFO) << "[DECODER] EXITED";
	}

	int width() { return decoder_ctx_ ? decoder_ctx_->width : 480; }
	int height() { return decoder_ctx_ ? decoder_ctx_->height : 360; }
	AVRational sar() { return decoder_ctx_ ? decoder_ctx_->sample_aspect_ratio : AVRational{ 0, 1 }; }
	AVRational framerate() { return opened() ? av_guess_frame_rate(fmt_ctx_, fmt_ctx_->streams[video_stream_index_], nullptr) : AVRational{30, 1}; }
	AVRational timebase() { return opened() ? fmt_ctx_->streams[video_stream_index_]->time_base : AVRational{1, AV_TIME_BASE}; }
	bool opened() { std::lock_guard<std::mutex> lock(mtx_); return opened_; }

signals:
	void started();
	void received();
	void stopped();

public slots:
	void pause() { std::lock_guard<std::mutex> lock(mtx_); paused_ = true; }
	void resume() { std::lock_guard<std::mutex> lock(mtx_); paused_ = false; }

	void stop() { std::lock_guard<std::mutex> lock(mtx_); LOG(INFO) << "[DEOCDER] STOPED"; running_ = false; }

	int read(AVFrame* frame)
	{
		int ret = -1;
		if (!running()) return -2;
		if (buffer_.unused()) return -3;

		buffer_.pop([frame, &ret](AVFrame* popped) {
			if (popped) {
				frame->height = popped->height;
				frame->width = popped->width;
				frame->format = popped->format;
				frame->pts = popped->pts;
				ret = av_frame_copy(frame, popped);
				if (ret < 0) {
					char buffer[AV_ERROR_MAX_STRING_SIZE]{ 0 };
					LOG(ERROR) << "av_frame_copy : " << av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, ret);
					LOG(ERROR) << "frame info: w= " << popped->width << ", h=" << popped->height << ", f=" << popped->format;
				}
			}
		});
		return ret;
	}

	int read(QImage& image) {
		buffer_.pop([&image](auto&& popped) {
			image = QImage(static_cast<const uchar*>(popped->data[0]), popped->width, popped->height, QImage::Format_RGB888).copy();
		});
		return 0;
	}

private:
	void close()
	{
		running(false);
		opened(false);

		first_pts_ = AV_NOPTS_VALUE;

		av_packet_free(&packet_);
		av_frame_free(&frame_);

		buffer_.clear();
		for (size_t i = 0; i < FRAME_BUFFER_SIZE; i++) {
			av_frame_free(&buffer_[i]);
		}

		avcodec_free_context(&decoder_ctx_);
		avformat_close_input(&fmt_ctx_);

		LOG(INFO) << "DECODER CLOSED";
	}

	void running(bool v) { std::lock_guard<std::mutex> lock(mtx_); running_ = v; }
	void opened(bool v) { std::lock_guard<std::mutex> lock(mtx_); opened_ = v; }

	bool running_{ false };
	bool paused_{ false };
	bool opened_{ false };

	AVFormatContext* fmt_ctx_{ nullptr };
	int video_stream_index_{ -1 };

	AVCodecContext* decoder_ctx_{ nullptr };
	AVCodec* decoder_{ nullptr };

	AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };

	AVPacket* packet_{ nullptr };
	AVFrame* frame_{ nullptr };
	AVFrame* rgb_frame_{ nullptr };

	SwsContext* sws_ctx_{ nullptr };

	int64_t first_pts_{ AV_NOPTS_VALUE };
	std::mutex mtx_;

	RingBuffer<AVFrame *, FRAME_BUFFER_SIZE> buffer_;
};

#endif // !CAPTURER_MEDIA_DECODER
