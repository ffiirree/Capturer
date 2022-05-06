#include "mediadecoder.h"
#include <fmt/core.h>

bool MediaDecoder::open(const std::string& name, const std::string& format, const string& filters_descr, AVPixelFormat pix_fmt, const std::map<std::string, std::string>& options)
{
	pix_fmt_ = pix_fmt;
	filters_descr_ = filters_descr;

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

	// filter graph
	if (!create_filters()) {
		LOG(ERROR) << "create_filters";
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
	
	filtered_frame_ = av_frame_alloc();
	if (!filtered_frame_) {
		LOG(ERROR) << "av_frame_alloc";
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

bool MediaDecoder::create_filters()
{
	// filters
	filter_graph_ = avfilter_graph_alloc();
	if (!filter_graph_) {
		LOG(ERROR) << "avfilter_graph_alloc";
		return false;
	}

	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	if (!buffersrc || !buffersink) {
		LOG(ERROR) << "avfilter_get_by_name";
		return false;
	}

	char args[512];
	AVStream* video_stream = fmt_ctx_->streams[video_stream_index_];
	sprintf(
		args,
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		decoder_ctx_->width, decoder_ctx_->height, decoder_ctx_->pix_fmt,
		video_stream->time_base.num, video_stream->time_base.den,
		video_stream->sample_aspect_ratio.num, video_stream->sample_aspect_ratio.den
	);

	LOG(INFO) << "[DECODER] " << "buffersrc args : " << args;

	if (avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in", args, nullptr, filter_graph_) < 0) {
		LOG(ERROR) << "avfilter_graph_create_filter";
		return false;
	}
	if (avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out", nullptr, nullptr, filter_graph_) < 0) {
		LOG(ERROR) << "avfilter_graph_create_filter";
		return false;
	}
	enum AVPixelFormat pix_fmts[] = { pix_fmt_, AV_PIX_FMT_NONE };
	if (av_opt_set_int_list(buffersink_ctx_, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
		LOG(ERROR) << "av_opt_set_int_list";
		return false;
	}
	
	if (filters_descr_.length() > 0) {
		AVFilterInOut* inputs = nullptr;
		AVFilterInOut* outputs = nullptr;
		defer(avfilter_inout_free(&inputs));
		defer(avfilter_inout_free(&outputs));
		if (avfilter_graph_parse2(filter_graph_, filters_descr_.c_str(), &inputs, &outputs) < 0) {
			LOG(ERROR) << "avfilter_graph_parse2";
			return false;
		}

		for (auto ptr = inputs; ptr; ptr = ptr->next) {
			if (avfilter_link(buffersrc_ctx_, 0, ptr->filter_ctx, ptr->pad_idx) != 0) {
				LOG(ERROR) << "avfilter_link";
				return false;
			}
		}

		for (auto ptr = outputs; ptr; ptr = ptr->next) {
			if (avfilter_link(ptr->filter_ctx, ptr->pad_idx, buffersink_ctx_, 0) != 0) {
				LOG(ERROR) << "avfilter_link";
				return false;
			}
		}
	}
	else {
		if (avfilter_link(buffersrc_ctx_, 0, buffersink_ctx_, 0) != 0) {
			LOG(ERROR) << "avfilter_link";
			return false;
		}
	}

	if (avfilter_graph_config(filter_graph_, nullptr) < 0) {
		LOG(ERROR) << "avfilter_graph_config(filter_graph_, nullptr)";
		return false;
	}

	LOG(INFO) << "[ENCODER] " << "filter graph @{";
	LOG(INFO) << "\n" << avfilter_graph_dump(filter_graph_, nullptr);
	LOG(INFO) << "[ENCODER] " << "@}";
	return true;
}

void MediaDecoder::process()
{
	LOG(INFO) << "[DECODER] STARTED@" << QThread::currentThreadId();
	LOG(INFO) << fmt::format("[DECODER] FRAMERATE = {}, CFR = {}, STREAM_TIMEBASE = {}/{}",
		decoder_ctx_->framerate.num, decoder_ctx_->framerate.num == decoder_ctx_->time_base.den, 
		decoder_ctx_->time_base.num, decoder_ctx_->time_base.den
	);


	if (!opened()) return;

	running(true);
	emit started();

	while (running()) {
		if (paused()) {
			QThread::msleep(20);
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

			if (av_buffersrc_add_frame_flags(buffersrc_ctx_, frame_, AV_BUFFERSRC_FLAG_PUSH) < 0) {
				LOG(ERROR) << "av_buffersrc_add_frame(buffersrc_ctx_, frame_)";
				break;
			}

			while (true) {
				ret = av_buffersink_get_frame_flags(buffersink_ctx_, filtered_frame_, AV_BUFFERSINK_FLAG_NO_REQUEST);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				if (ret < 0) {
					char buffer[AV_ERROR_MAX_STRING_SIZE]{ 0 };
					LOG(ERROR) << "av_buffersink_get_frame_flags : " << av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, ret);
					break;
				}

				first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

				// decoded frame@{
				// convert the format to AV_PIX_FMT_XXXX
				buffer_.push(
					[this](AVFrame* buffer_frame) {
						buffer_frame->height = filtered_frame_->height;
						buffer_frame->width = filtered_frame_->width;
						buffer_frame->format = filtered_frame_->format;
						av_frame_copy(buffer_frame, filtered_frame_);

						buffer_frame->pts = av_rescale_q(av_gettime_relative() - first_pts_, { 1, AV_TIME_BASE }, decoder_ctx_->time_base);
					}
				);

				emit received();
				// @}

				av_frame_unref(filtered_frame_);
			}

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
	resume();

	first_pts_ = AV_NOPTS_VALUE;

	avfilter_graph_free(&filter_graph_);

	av_packet_free(&packet_);
	av_frame_free(&frame_);
	av_frame_free(&filtered_frame_);

	buffer_.clear();
	for (size_t i = 0; i < FRAME_BUFFER_SIZE; i++) {
		av_frame_free(&buffer_[i]);
	}

	avcodec_free_context(&decoder_ctx_);
	avformat_close_input(&fmt_ctx_);

	LOG(INFO) << "DECODER CLOSED";
}