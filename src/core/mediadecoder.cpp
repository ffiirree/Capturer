#include "mediadecoder.h"

bool MediaDecoder::open(const std::string& name, const std::string& format, AVPixelFormat pix_fmt, const std::map<std::string, std::string>& options)
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

void MediaDecoder::process()
{
	LOG(INFO) << "[DECODER] STARTED@" << QThread::currentThreadId();
	LOG(INFO) << "[DECODER] FRAMERATE = " << decoder_ctx_->framerate.num << ", CFR = " << (decoder_ctx_->framerate.num == decoder_ctx_->time_base.den) << ", TIMEBASE = " << decoder_ctx_->time_base.num << "/" << decoder_ctx_->time_base.den;


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
			// convert the format to AV_PIX_FMT_XXXX
			buffer_.push(
				[this](AVFrame* buffer_frame) {
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
				}
			);

			emit received();
			// @}

			av_frame_unref(frame_);
		}
		av_packet_unref(packet_);
	}

	LOG(INFO) << "[DECODER] " << "decoded frames: " << decoder_ctx_->frame_number << ", fps = " << decoder_ctx_->frame_number / (av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, { 1, 1 }));

	close();
	emit stopped();
	LOG(INFO) << "[DECODER] EXITED";
}

void MediaDecoder::close()
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