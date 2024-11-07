#include "XPlayer.h"
#include "XDemux.h"
#include "XDecode.h"
#include <QtWidgets/QApplication>
#include <windows.h>
#include <QThread>

class TestThread:public QThread
{
public:
	TestThread() {}
	~TestThread() {}
	void init() {
		const char* url = "src_video.mp4";
		xDemux.Open(url);
		xDemux.Seek(0.7);
		/*打开解码器*/
		if (x_vDecode.Open(xDemux.CopyVPara())) {
			std::cout << "[Video]" << std::endl;
		}
		if (x_aDecode.Open(xDemux.CopyAPara())) {
			std::cout << "[Audio]" << std::endl;
		}
	}
	void run() {
		/*解码*/
		for (;;) {
			AVPacket* pkt = xDemux.Read();
			if (!pkt)
				break;
			if (xDemux.IsAudio(pkt)) {
				//x_aDecode.Send(pkt);
				//AVFrame* frame = x_aDecode.Recv();
				//if (frame)
				//	av_frame_free(&frame);
			}
			else {
				x_vDecode.Send(pkt);
				AVFrame* frame = x_vDecode.Recv();
				videoWidget->Repaint(frame);
				/*if (frame)
					av_frame_free(&frame);*/
			}
		}
	}
public:
	XDemux xDemux;
	XDecode x_vDecode;
	XDecode x_aDecode;
	XVideoWidget* videoWidget = nullptr;
};

int main(int argc, char *argv[])
{
	// 设置控制台输出编码为 UTF-8
	SetConsoleOutputCP(CP_UTF8);
	TestThread tt;
	tt.init();

	QApplication a(argc, argv);
	XPlayer w;
	w.show();
	/*初始化gl窗口*/
	w.GetUI()->openGLWidget->Init(tt.xDemux.width_, tt.xDemux.height_);
	tt.videoWidget = w.GetUI()->openGLWidget;
	tt.start();
    return a.exec();
}
