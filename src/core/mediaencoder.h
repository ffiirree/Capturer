#ifndef CAPTURER_MEDIA_ENCODER_H
#define CAPTURER_MEDIA_ENCODER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}
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

	bool open(const string& filename, const string& codec_name, AVPixelFormat pix_fmt, AVRational framerate = { 25, 1 }, bool is_cfr = false, const map<string, string>& options = {});

	void process();

	int width() { return encoder_ctx_ ? encoder_ctx_->width : 0; }
	int height() { return encoder_ctx_ ? encoder_ctx_->height : 0; }
	bool opened() { std::lock_guard<std::mutex> lock(mtx_); return opened_; }

	// ms
	int64_t escaped_ms() { return av_rescale_q(escaped(), {1, AV_TIME_BASE}, {1, 1000}); }

	// us
	int64_t escaped() { return (first_pts_ == AV_NOPTS_VALUE) ? 0 : av_gettime_relative() - first_pts_ - offset_pts_; }

signals:
	void started();
	void stopped();

private:
	void close();

	void opened(bool v) { std::lock_guard<std::mutex> lock(mtx_); opened_ = v; }

	bool opened_{ false };

	MediaDecoder* decoder_{ nullptr };
	AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };

	AVFormatContext* fmt_ctx_{ nullptr };
	int video_stream_index_{ -1 };

	AVCodecContext* encoder_ctx_{ nullptr };
	AVCodec* encoder_{ nullptr };

	AVPacket* packet_{ nullptr };
	AVFrame* decoded_frame_{ nullptr };

	int64_t first_pts_{ AV_NOPTS_VALUE };
	std::mutex mtx_;

	bool is_cfr_{ false };
	string filename_{};

	int64_t paused_pts_{ AV_NOPTS_VALUE };
	int64_t offset_pts_{ 0 };
};
#endif // !CAPTURER_MEDIA_ENCODER_H
