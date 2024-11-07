#pragma once
//@time 2024/11/6/19:29 
#include <iostream>	//如果少了输入输出流也会报错
#include <mutex>
#include <unordered_map>
//库外文件必须在后面含入，不能先于本机头文件
/*动态库在.cpp文件中链入，因为.cpp文件只会被编译一次*/
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
/*Seek时候方案*/
enum SEEK_CHOICE {
	SEEK_AVSTREAM_DURATION = 0,
	SEEK_NB_STREAM = 1,
	SEEK_AVFORMATCONTEXT_TIME_BASE = 2,
};
class XDemux
{
public:
	XDemux();
	virtual ~XDemux();
public:
	double r2d(AVRational r);

	/// <summary>
	/// 打卡多媒体文件或者流媒体（rtmp、http、rtsp、ETC）
	/// </summary>
	/// <param name="url">打开源</param>
	/// <returns></returns>
	virtual bool Open(const char* url);

	/// <summary>
	/// 为了保证多线程安全，因此内部分配空间，因此外部需要释放整个空间:av_packet_free
	/// </summary>
	/// <returns>已分配好内存的AVPacket*</returns>
	virtual AVPacket* Read();

	/// <summary>
	///  获取视频参数，内部分配了空间，外部需要avocdec_parameters_free()来清理
	/// </summary>
	/// <returns>返回AVCodecParameters*，，外部需要avocdec_parameters_free()</returns>
	virtual AVCodecParameters* CopyVPara();

	/// <summary>
	/// 获取音频参数，内部分配了空间，外部需要avocdec_parameters_free()来清理
	/// </summary>
	/// <returns>返回AVCodecParameters*，，外部需要avocdec_parameters_free()</returns>
	virtual AVCodecParameters* CopyAPara();

	/// <summary>
	/// 视频移动位置，pos:[0,1],仅仅往后调到关键帧，跳到实际帧得外部操作；并且如果当前视频的AVStream的duration为空还得另外处理
	/// </summary>
	/// <param name="pos">位置百分比</param>
	/// <param name="pos">Seek的方案，默认是SEEK_AVSTREAM_DURATION</param>
	/// <returns>bool</returns>
	virtual bool Seek(double pos, SEEK_CHOICE choice = SEEK_AVSTREAM_DURATION);

	/// <summary>
	/// 清空读取缓存
	/// </summary>
	virtual void Clear();

	/// <summary>
	/// 回收清空设置0
	/// </summary>
	virtual void Close();

	/// <summary>
	/// 判断是否是音频流
	/// </summary>
	/// <param name="pkt">av_read_frame读取到的AVPacket*</param>
	/// <returns>是否 bool</returns>
	bool IsAudio(AVPacket* pkt);

	/// <summary>
	/// 将FFmpeg返回的错误码转化为错误信息
	/// </summary>
	/// <param name="errnum">错误码</param>
	void StringErr(const int& errnum);
private:
	/*C++11以后可以放在类内初始化*/
	/*参数设置*/
	std::mutex Gmtx_;
	/*管控错误信息的锁*/
	std::mutex Emtx_;
	/*输入源(内部分配空间)*/
	char* url_ = nullptr;
	/*输入源上下文*/
	AVFormatContext* ic_ = nullptr;
	AVDictionary* opts_ = nullptr;
	/*流序号*/
	int videoStreamIndex_ = -1;
	int audioStreamIndex_ = -1;
	/*AVStream序号*/
	AVStream* videoS = nullptr;
	AVStream* audioS = nullptr;
	/*解码器上下文（序号->解码器上下文）*/
	std::unordered_map<int, AVCodecContext*> codec_map_;
public:
	/*解码后的yuv宽高用以openGL绘制*/
	int width_ = 0;
	int height_ = 0;
	/*总时长(毫秒)*/
	int64_t totalMs_ = 0;
	/*描述声道*/
	char channel_human[1024];
	/*错误err*/
	char err_[1024] = { 0 };
	bool isClose = false;
};

