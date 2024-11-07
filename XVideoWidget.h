#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QTimer>
#include <mutex>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
class XVideoWidget  : public QOpenGLWidget, protected  QOpenGLFunctions
{
	Q_OBJECT

public:
	XVideoWidget(QWidget *parent);
	~XVideoWidget();

	/// <summary>
	/// 初始化窗口宽高,不管成功与否都释放AVFrame*
	/// </summary>
	/// <param name="width">yuv数据的宽</param>
	/// <param name="height">yuv数据的高</param>
	void Init(int width, int height);

	/// <summary>
	/// 接受解码后的yuv数据
	/// </summary>
	/// <param name="frame">解码后的frame</param>
	virtual void Repaint(AVFrame* frame);
protected:

	/// <summary>
	/// 刷新显示
	/// </summary>
	void paintGL();

	/// <summary>
	/// 初始化GL
	/// </summary>
	void initializeGL();

	/// <summary>
	/// 窗口尺寸变换
	/// </summary>
	/// <param name="width">宽</param>
	/// <param name="height">高</param>
	void resizeGL(int width,int height);
private:
	std::mutex Gmtx_;
	/*shader程序*/
	QOpenGLShaderProgram program;
	/*shader中的yuv变量地址*/
	GLuint unis[3] = { 0 };
	/*openGL中的texture地址*/
	GLuint texs[3] = { 0 };
	/*材质空间地址*/
	unsigned char* datas[3] = { 0 };
	/*yuv420P数据的宽高*/
	int width = 3840;
	int height = 2160;
	/*FILE* fp;*/
};
