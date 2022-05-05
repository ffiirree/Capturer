#ifndef CAPTURER_MEDIA_DECODER
#define CAPTURER_MEDIA_DECODER

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
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
	enum { BUFFER_UNUSED = 1, BUFFER_USED_EMPTY, BUFFER_FULL, UNRUNNING };

public:
	explicit MediaDecoder() {}
	~MediaDecoder()
	{
		close();
	}

	bool open(const string& name, const string& format, const string& filters_descr, AVPixelFormat pix_fmt, const map<string, string>& options);
	bool create_filters();

	bool running() { std::lock_guard<std::mutex> lock(mtx_); return running_; }
	bool paused() { std::lock_guard<std::mutex> lock(mtx_); return paused_; }

	void process();
	
	int width() { return decoder_ctx_ ? decoder_ctx_->width : 480; }
	int height() { return decoder_ctx_ ? decoder_ctx_->height : 360; }
	AVRational sar() { return decoder_ctx_ ? decoder_ctx_->sample_aspect_ratio : AVRational{ 0, 1 }; }
	AVRational framerate() { return opened() ? av_guess_frame_rate(fmt_ctx_, fmt_ctx_->streams[video_stream_index_], nullptr) : AVRational{ 30, 1 }; }
	AVRational timebase() { return opened() ? fmt_ctx_->streams[video_stream_index_]->time_base : AVRational{ 1, AV_TIME_BASE }; }
	bool opened() { std::lock_guard<std::mutex> lock(mtx_); return opened_; }

signals:
	void started();
	void received();
	void stopped();

public slots:
	void pause() { std::lock_guard<std::mutex> lock(mtx_); paused_ = true; }
	void resume() { std::lock_guard<std::mutex> lock(mtx_); paused_ = false; }

	void stop() { std::lock_guard<std::mutex> lock(mtx_); running_ = false; }

	// = 0 on success
	// < 0 AVERROR
	// > 0 
	int read(AVFrame* frame)
	{
		int ret = 0;
		if (buffer_.full()) ret = MediaDecoder::BUFFER_FULL;
		if (!running()) return UNRUNNING;
		if (buffer_.unused()) return BUFFER_UNUSED;
		if (buffer_.empty() && !buffer_.unused()) ret = BUFFER_USED_EMPTY;
		
		buffer_.pop(
			[frame, &ret](AVFrame* popped) {
				if (popped) {
					frame->height = popped->height;
					frame->width = popped->width;
					frame->format = popped->format;
					frame->pts = popped->pts;
					ret = av_frame_copy(frame, popped);
					if (ret < 0) {
						char buffer[AV_ERROR_MAX_STRING_SIZE]{ 0 };
						LOG(ERROR) << "av_frame_copy : " << av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, ret);
						LOG(ERROR) << "error frame info: w= " << popped->width << ", h=" << popped->height << ", f=" << popped->format;
					}
				}
			});
		return ret;
	}

	int read(QImage& image) {
		int ret = 0;
		if (buffer_.full()) ret = MediaDecoder::BUFFER_FULL;
		if (!running()) return UNRUNNING;
		if (buffer_.unused()) return BUFFER_UNUSED;
		if (buffer_.empty() && !buffer_.unused()) ret = BUFFER_USED_EMPTY;

		buffer_.pop(
			[&image](AVFrame* popped) {
				image = QImage(static_cast<const uchar*>(popped->data[0]), popped->width, popped->height, QImage::Format_RGB888).copy();
			}
		);
		return ret;
	}

private:
	void close();

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
	AVFrame* filtered_frame_{ nullptr };

	int64_t first_pts_{ AV_NOPTS_VALUE };
	std::mutex mtx_;

	RingBuffer<AVFrame*, FRAME_BUFFER_SIZE> buffer_;

	string filters_descr_;
	AVFilterGraph* filter_graph_{ nullptr };
	AVFilterContext* buffersrc_ctx_{ nullptr };
	AVFilterContext* buffersink_ctx_{ nullptr };
};

#endif // !CAPTURER_MEDIA_DECODER
