#include "XDemux.h"
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avdevice.lib")
#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avutil.lib")
#pragma comment (lib, "swresample.lib")
#pragma comment (lib, "swscale.lib")
XDemux::XDemux()
{
	static bool isFirst = true;
	static std::mutex mutexF;
	mutexF.lock();
	if (isFirst) {
		av_log_set_level(AV_LOG_DEBUG);
		/*初始化封装库（已经弃用）*/
		//av_register_all();
		/*初始化网络库，可以打开网络流*/
		avformat_network_init();
		isFirst = false;
	}
	mutexF.unlock();
}

XDemux::~XDemux()
{
	if(!isClose)
		Close();
	isClose = false;
}	

double XDemux::r2d(AVRational r)
{
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

bool XDemux::Open(const char* url)
{
	/*先清理上一次的资源*/
	Close();
	/*防止死锁*/
	/*保证多线程安全*/
	std::lock_guard<std::mutex> lck(Gmtx_);

	url_ = av_strdup(url);
	if (!url_) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "URL Allocated Failed");
		return false;
	}
	/*设置rtsp流已经由tcp协议打开*/
	av_dict_set(&opts_, "rtsp_transport", "tcp", 0);
	/*网络延时时间*/
	av_dict_set(&opts_, "max_delay", "500", 0);
	
	/* x x 自动选择解封装器 参数设置（比如rtsp的延时时间）*/
	int ret = avformat_open_input(&ic_, url_, 0, &opts_);
	if (ret != 0) {
		StringErr(ret);
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, err_);
		return false;
	}
	/*获取流信息*/
	ret = avformat_find_stream_info(ic_, 0);

	totalMs_ = ic_->duration / (AV_TIME_BASE / 1000);
	std::cout << "Total Time(ms) : " << totalMs_ << std::endl;
	
	/*打印视频流详细信息*/
	av_dump_format(ic_, 0, url_, 0);
	
	/*获取流信息*/
	for (int i = 0; i < ic_->nb_streams; ++i) {
		AVStream* as = ic_->streams[i];
		/*音频*/
		if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioS = as;
			audioStreamIndex_ = i;
			std::cout << "============音频信息============" << std::endl;
			std::cout << "codec_id : " << as->codecpar->codec_id << std::endl;
			std::cout << "format : " << as->codecpar->format << std::endl;
			std::cout << "sample_rate = " << as->codecpar->sample_rate << std::endl;
			/*std::cout << "channels" << as->codecpar->ch_layout.u.mask << std::endl;*/
			av_channel_layout_describe(&as->codecpar->ch_layout,channel_human,sizeof(channel_human));
			std::cout << "channel_layout : " << channel_human << std::endl;
			std::cout << "frame_size : " << as->codecpar->frame_size << std::endl;
		}
		/*视频*/
		else if(as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoS = as;
			videoStreamIndex_ = i;
			width_ = as->codecpar->width;
			height_ = as->codecpar->height;
			std::cout << "============视频信息============" << std::endl;
			std::cout << "codec_id : " << as->codecpar->codec_id << std::endl;
			std::cout << "format : " << as->codecpar->format << std::endl;
			std::cout << "width = " << as->codecpar->width << std::endl;
			std::cout << "height = " << as->codecpar->height << std::endl;
			/*帧率 fps 分数转换*/
			std::cout << "video fps = " << r2d(as->avg_frame_rate) << std::endl;
		}
	}
	/*获取视频流*/
	//videoStreamIndex = av_find_best_stream(ic_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	return true;
}

AVPacket* XDemux::Read()
{
	std::lock_guard<std::mutex> lck(Gmtx_);

	/*没有打开*/
	if (!ic_) {
		return nullptr;
	}
	/*每次都开辟空间会不会很耗时？*/
	AVPacket* pkt = av_packet_alloc();
	if (!pkt) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "Couldn't Allcated AVPacket");
		return nullptr;
	}
	int ret = av_read_frame(ic_, pkt);
	if (ret != 0) {
		StringErr(ret);
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, err_);
		av_packet_free(&pkt);
		return nullptr;
	}
	/*pts转化为毫秒*/
	pkt->pts = pkt->pts * (1000 * r2d(ic_->streams[pkt->stream_index]->time_base));
	/*dts转化为毫秒*/
	pkt->dts = pkt->dts * (1000 * r2d(ic_->streams[pkt->stream_index]->time_base));
	std::cout << "pkt->pts : " << pkt->pts << std::endl;
	return pkt;
}

AVCodecParameters* XDemux::CopyVPara()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (!ic_) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVFormatContext* is NULL");
		return nullptr;
	}
	AVCodecParameters* vPara = avcodec_parameters_alloc();
	if (!vPara) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVCodecParameters* is NULL");
		return nullptr;
	}
	avcodec_parameters_copy(vPara, videoS->codecpar);
	std::cout << "CopyVPara : " << vPara << std::endl;
	return vPara;
}

AVCodecParameters* XDemux::CopyAPara()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (!ic_) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVFormatContext* is NULL");
		return nullptr;
	}
	AVCodecParameters* aPara = avcodec_parameters_alloc();
	if (!aPara) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVCodecParameters* is NULL");
		return nullptr;
	}
	avcodec_parameters_copy(aPara, audioS->codecpar);
	std::cout << "CopyAPara : " << aPara << std::endl;
	return aPara;
}

bool XDemux::Seek(double pos,SEEK_CHOICE choice)
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (!ic_) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVFormatContext* is NULL");
		return false;
	}
	
	/*不要调用Clear()---会死锁*/
	/*清理缓冲，防止粘包*/
	avformat_flush(ic_);
	long long seekPos = videoS->duration * pos;
	int ret = av_seek_frame(ic_, videoStreamIndex_, seekPos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
	if (ret < 0) {
		return false;
	}
	//if (choice == SEEK_AVSTREAM_DURATION) {
	//	// 使用 videoS->duration 来进行跳转
	//	if (videoS->duration <= 0) {
	//		/*如果默认方案不行则自动跳转至第二个方案*/
	//		goto second;
	//	}
	//	long long targetPosInMicroseconds = videoS->duration * pos;
	//	long long seekTimestamp = av_rescale_q(targetPosInMicroseconds, AV_TIME_BASE_Q, videoS->time_base);
	//	int ret = av_seek_frame(ic_, videoStreamIndex_, seekTimestamp, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
	//	if (ret < 0) {
	//		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "Seek failed");
	//		return false;
	//	}
	//}
	//else if (choice == SEEK_NB_STREAM) {
	//	second:
	//	// 使用视频流的帧数来进行跳转
	//	if (videoS->nb_frames <= 0) {
	//		/*如果第二个方案不行则自动跳转至第三个方案*/
	//		goto third;
	//	}
	//	int64_t targetFrame = static_cast<int64_t>(videoS->nb_frames * pos);
	//	int64_t seekTimestamp = av_rescale_q(targetFrame, AVRational{ 1, 1 }, videoS->time_base);
	//	int ret = av_seek_frame(ic_, videoStreamIndex_, seekTimestamp, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
	//	if (ret < 0) {
	//		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "Seek failed");
	//		return false;
	//	}
	//}
	//else if (choice == SEEK_AVFORMATCONTEXT_TIME_BASE) {
	//	third:
	//	// 使用 AVFormatContext 的时间基准来进行换算
	//	if (ic_->duration <= 0) {
	//		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "Neither stream nor format context has valid duration");
	//		return false;
	//	}
	//	long long targetPosInMicroseconds = ic_->duration * pos;
	//	long long seekTimestamp = av_rescale_q(targetPosInMicroseconds, AV_TIME_BASE_Q, videoS->time_base);
	//	int ret = av_seek_frame(ic_, videoStreamIndex_, seekTimestamp, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
	//	if (ret < 0) {
	//		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "Seek failed");
	//		return false;
	//	}
	//}
	return true;
}

void XDemux::Clear()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (!ic_) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVFormatContext* is NULL");
		return;
	}
	/*清空读取缓存*/
	avformat_flush(ic_);
	return;
}

void XDemux::Close()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (!ic_) {
		return;
	}
	if (url_) {
		av_free(url_);
		url_ = nullptr;
	}
	if (ic_) {
		/*不需要再Clear()*/
		avformat_close_input(&ic_);
	}
	if (opts_) {
		av_dict_free(&opts_);
		opts_ = nullptr;
	}
	for (auto& it : codec_map_) {
		avcodec_free_context(&it.second);
		it.second = nullptr;
	}
	videoS = nullptr;
	audioS = nullptr;
	videoStreamIndex_ = -1;
	audioStreamIndex_ = -1;
	width_ = 0;
	height_ = 0;
	totalMs_ = 0;
	isClose = true;
}

bool XDemux::IsAudio(AVPacket* pkt)
{
	if (!pkt)
		return false;
	if (pkt->stream_index == videoStreamIndex_) {
		return false;
	}
	return true;
}

void XDemux::StringErr(const int& errnum)
{
	std::lock_guard<std::mutex> lck(Emtx_);
	av_strerror(errnum, err_, sizeof(err_));
	return;
}

