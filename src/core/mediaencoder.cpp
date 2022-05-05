#include "mediaencoder.h"

bool MediaEncoder::open(const string& filename, const string& codec_name, AVPixelFormat pix_fmt, AVRational framerate, bool is_cfr, const map<string, string>& options)
{
	pix_fmt_ = pix_fmt;
	is_cfr_ = is_cfr;

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

	encoder_ctx_->time_base = is_cfr ? av_inv_q(framerate) : decoder_->timebase();
	fmt_ctx_->streams[video_stream_index_]->time_base = is_cfr ? av_inv_q(framerate) : decoder_->timebase();
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

void MediaEncoder::process()
{
	LOG(INFO) << "[ENCODER] STARTED@" << QThread::currentThreadId();
	LOG(INFO) << "[ENCODER] FRAMERATE = " << encoder_ctx_->framerate.num << "/" << encoder_ctx_->framerate.den << ", CFR = " << is_cfr_ << ", TIMEBASE = " << encoder_ctx_->time_base.num << "/" << fmt_ctx_->streams[video_stream_index_]->time_base.den;

	if (!opened() || !decoder_ || !decoder_->opened()) return;

	emit started();

	first_pts_ = AV_NOPTS_VALUE;
	int64_t duration = av_q2d(av_div_q(av_inv_q(encoder_ctx_->framerate), { 1, AV_TIME_BASE })); // in AV_TIME_BASE unit
	LOG(INFO) << "duration = " << duration;
	while (decoder_ && decoder_->running()) {
		if (decoder_->paused()) {
			QThread::msleep(20);
			continue;
		}

		int ret = decoder_->read(decoded_frame_);
		if (ret < 0 || 
			ret == MediaDecoder::BUFFER_UNUSED || 
			ret == MediaDecoder::UNRUNNING || 
			(ret == MediaDecoder::BUFFER_USED_EMPTY && !is_cfr_)) {
			QThread::msleep(20);
			continue;
		}
		
		first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

		decoded_frame_->pts = is_cfr_ ? encoder_ctx_->frame_number : av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, encoder_ctx_->time_base); ;

		int64_t delta = std::max<int64_t>(0, encoder_ctx_->frame_number * duration - (av_gettime_relative() - first_pts_));
		int64_t next_pts = av_gettime_relative() + std::min<int64_t>(static_cast<int64_t>(duration * 1.1), is_cfr_ ? delta : (delta / 3)); 

		ret = avcodec_send_frame(encoder_ctx_, decoded_frame_);
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
			packet_->dts = packet_->pts;
			packet_->duration = av_rescale_q(duration, { 1, AV_TIME_BASE }, encoder_ctx_->time_base);

			av_packet_rescale_ts(packet_, encoder_ctx_->time_base, fmt_ctx_->streams[video_stream_index_]->time_base);

			ret = av_interleaved_write_frame(fmt_ctx_, packet_);
			if (ret != 0) {
				LOG(ERROR) << "av_interleaved_write_frame";
				return;
			}
		}

		QThread::usleep(std::max<int64_t>(0, next_pts - av_gettime_relative()));
	}

	LOG(INFO) << "[ENCODER]" << " encoded frames: " << encoder_ctx_->frame_number << ", fps = " << encoder_ctx_->frame_number / (av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, { 1, 1 }));

	close();
	emit stopped();
	LOG(INFO) << "[ENCODER] EXITED ";
}

void MediaEncoder::close()
{
	opened(false);

	first_pts_ = AV_NOPTS_VALUE;

	int ret = av_write_trailer(fmt_ctx_);
	if (ret != 0) {
		char buffer[AV_ERROR_MAX_STRING_SIZE]{ 0 };
		LOG(ERROR) << "av_write_trailer : " << av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, ret);
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