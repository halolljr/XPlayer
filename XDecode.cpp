#include "XDecode.h"
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avdevice.lib")
#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avutil.lib")
#pragma comment (lib, "swresample.lib")
#pragma comment (lib, "swscale.lib")

static int THREAD_COUNT = 8;
XDecode::XDecode()
{
	
}

XDecode::~XDecode()
{
	if(!isClose)
		Close();
	isClose = false;
}

bool XDecode::Open(AVCodecParameters* para)
{
	if (!para) {
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVCodecParameters* is NULL");
		return false;
	}
	Close();
	/*防止死锁*/
	std::lock_guard<std::mutex> lck(Gmtx_);
	/*解码器打开并找到解码器*/
	/*视频解码器*/
	codec_ = avcodec_find_decoder(para->codec_id);
	if (!codec_) {
		avcodec_parameters_free(&para);
		av_log(nullptr, AV_LOG_ERROR, "%s : %s\n", __FUNCTION__, "AVCodec* is NULL");
		return false;
	}
	std::cout << "Find the AVCodecID : " << para->codec_id << std::endl;

	ctx_ = avcodec_alloc_context3(codec_);
	/*配置解码器上下文参数*/
	avcodec_parameters_to_context(ctx_, para);
	/*八线程解码*/
	ctx_->thread_count = THREAD_COUNT;
	/*绑定解码器与解码器上下文并打开解码器上下文*/
	int ret = avcodec_open2(ctx_, 0, 0);
	if (ret != 0) {
		char ErrBuff[1024];
		av_strerror(ret, ErrBuff, sizeof(ErrBuff));
		av_log(nullptr, AV_LOG_ERROR, ErrBuff);
		avcodec_free_context(&ctx_);
		avcodec_parameters_free(&para);
		return false;
	}
	avcodec_parameters_free(&para);
	return true;
}

bool XDecode::Send(AVPacket* pkt)
{
	/*容错处理*/
	if (!pkt || pkt->size <= 0 || !pkt->data) {
		return false;
	}
	/*颗粒度更小*/
	Gmtx_.lock();
	if (!ctx_) {
		av_packet_free(&pkt);
		Gmtx_.unlock();
		return false;
	}
	int ret = avcodec_send_packet(ctx_, pkt);
	Gmtx_.unlock();
	av_packet_free(&pkt);
	if (ret != 0) {
		return false;
	}
	return true;
}

AVFrame* XDecode::Recv()
{
	Gmtx_.lock();
	if (!ctx_) {
		Gmtx_.unlock();
		return nullptr;
	}
	/*每次都需要开辟空间会不会很耗时？*/
	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		Gmtx_.unlock();
		return nullptr;
	}
	int ret = avcodec_receive_frame(ctx_, frame);
	Gmtx_.unlock();
	if (ret != 0) {
		av_frame_free(&frame);
		return nullptr;
	}
	std::cout << "Frame-LineSize[0] : " << frame->linesize[0] << std::endl;
	return frame;
}

void XDecode::Clear()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (ctx_) {
		avcodec_flush_buffers(ctx_);
	}
	return;
}

void XDecode::Close()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	if (ctx_) {
		avcodec_free_context(&ctx_);
	}
	isClose = true;
	return;
}
