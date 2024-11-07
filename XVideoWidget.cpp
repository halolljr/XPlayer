#include "XVideoWidget.h"
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avdevice.lib")
#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avutil.lib")
#pragma comment (lib, "swresample.lib")
#pragma comment (lib, "swscale.lib")
/*自动加双引号*/
#define GET_STR(x) #x
#define A_VER 3
#define T_VER 4
/*顶点shader*/
const char* vString = GET_STR(	attribute vec4 vertexIn;
								attribute vec2 textureIn;
								varying vec2 textureOut;
								void main(void) {
									gl_Position = vertexIn;
									textureOut = textureIn;
								}
);

/*片元shader*/
const char* tString = GET_STR(varying vec2 textureOut;
							uniform sampler2D tex_y;
							uniform sampler2D tex_u;
							uniform sampler2D tex_v;
							void main(void) {
								vec3 yuv;
								vec3 rgb;
								yuv.x = texture2D(tex_y, textureOut).r;
								yuv.y = texture2D(tex_u, textureOut).r - 0.5;
								yuv.z = texture2D(tex_v, textureOut).r - 0.5;
								rgb = mat3(1.0, 1.0, 1.0, 0.0, -0.39465, 2.03211, 1.13983, -0.58060, 0.0) * yuv;
								gl_FragColor = vec4(rgb, 1.0);
							}
);


XVideoWidget::XVideoWidget(QWidget *parent)
	: QOpenGLWidget(parent)
{}

XVideoWidget::~XVideoWidget()
{
	if (datas[0]) {
		delete datas[0];
		datas[0] = nullptr;
	}
	if (datas[1]) {
		delete datas[1];
		datas[1] = nullptr;
	}
	if (datas[2]) {
		delete datas[2];
		datas[2] = nullptr;
	}
	if (texs[0]) {
		glDeleteTextures(3, texs);
	};
}

void XVideoWidget::Init(int width, int height)
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	this->width = width;
	this->height = height;
	if (datas[0]) {
		delete datas[0];
		datas[0] = nullptr;
	}
	if (datas[1]) {
		delete datas[1];
		datas[1] = nullptr;
	}
	if (datas[2]) {
		delete datas[2];
		datas[2] = nullptr;
	}
	if (texs[0]) {
		glDeleteTextures(3, texs);
	}
	/*分配材质内存空间*/
	datas[0] = new unsigned char[width * height];	//Y
	datas[1] = new unsigned char[width * height / 4];	//U
	datas[2] = new unsigned char[width * height / 4];	//V

	
	/*创建材质*/
	glGenTextures(3, texs);
	//Y
	glBindTexture(GL_TEXTURE_2D, texs[0]);
	/*放大缩小过滤，GL_LINEAR线性插值(GL_NEAREST插值效率高但马赛克严重)*/
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	/*创建材质显卡空间*/
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

	//U
	glBindTexture(GL_TEXTURE_2D, texs[1]);
	/*放大缩小过滤，线性插值*/
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	/*创建材质显卡空间*/
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

	//V
	glBindTexture(GL_TEXTURE_2D, texs[2]);
	/*放大缩小过滤，线性插值*/
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	/*创建材质显卡空间*/
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
	return;
}

void XVideoWidget::Repaint(AVFrame* frame)
{
	if (!frame)
		return;
	Gmtx_.lock();
	/*容错，保证宽高正确*/
	if (!datas[0] ||width* height == 0 ||frame->width != this->width || frame->height != this->height) {
		av_frame_free(&frame);
		Gmtx_.unlock();
		return;
	}

	memcpy(datas[0], frame->data[0], width * height);
	memcpy(datas[1], frame->data[1], width * height / 4);
	memcpy(datas[2], frame->data[2], width * height / 4);
	/*行对齐问题*/
	Gmtx_.unlock();
	update();
}

void XVideoWidget::paintGL()
{
	std::lock_guard <std::mutex> lck(Gmtx_);
	//if (feof(fp)) {
	//	/*循环播放*/
	//	fseek(fp, 0, SEEK_SET);
	//}
	///*显示*/
	//fread(datas[0], 1, width * height, fp);
	//fread(datas[1], 1, width * height/4, fp);
	//fread(datas[2], 1, width * height/4, fp);
	/*激活0层材质*/
	glActiveTexture(GL_TEXTURE0);
	/*0层绑定到Y材质中*/
	glBindTexture(GL_TEXTURE_2D, texs[0]);
	/*修改材质内容（复制内存内容）*/
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, datas[0]);
	/*与shader uni遍历关联*/
	glUniform1i(unis[0], 0);

	/*激活1层材质*/
	glActiveTexture(GL_TEXTURE0+1);
	/*1层绑定到U材质中*/
	glBindTexture(GL_TEXTURE_2D, texs[1]);
	/*修改材质内容（复制内存内容）*/
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2, GL_RED, GL_UNSIGNED_BYTE, datas[1]);
	/*与shader uni遍历关联*/
	glUniform1i(unis[1], 1);

	/*激活2层材质*/
	glActiveTexture(GL_TEXTURE0+2);
	/*2层绑定到V材质中*/
	glBindTexture(GL_TEXTURE_2D, texs[2]);
	/*修改材质内容（复制内存内容）*/
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2, GL_RED, GL_UNSIGNED_BYTE, datas[2]);
	/*与shader uni遍历关联*/
	glUniform1i(unis[2], 2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	return;
}

void XVideoWidget::initializeGL()
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	/*初始化openGL(QOpenGLFunctions继承)的函数*/
	initializeOpenGLFunctions();

	/*program加载shader（顶点和片元）脚本*/
	/*片元(像素)*/
	program.addShaderFromSourceCode(QOpenGLShader::Fragment, tString);
	/*顶点shader*/
	program.addShaderFromSourceCode(QOpenGLShader::Vertex, vString);

	/*设置顶点坐标的变量*/
	program.bindAttributeLocation("vertexIn", A_VER);
	/*设置材质坐标*/
	program.bindAttributeLocation("textureIn", T_VER);

	/*编译shader*/
	program.link();
	/*绑定*/
	program.bind();

	/*传递顶点和材质坐标*/
	/*顶点*/
	static const GLfloat ver[] = { -1.0f,-1.0f,
								  1.0f,-1.0f,
								  -1.0f,1.0f,
								  1.0f,1.0f };
	/*材质*/
	static const GLfloat tex[] = { 0.0f,1.0f,
								  1.0f,1.0f,
								  0.0f,0.0f,
								  1.0f,0.0f };
	/*顶点*/
	glVertexAttribPointer(A_VER, 2, GL_FLOAT, 0, 0, ver);
	glEnableVertexAttribArray(A_VER);

	/*材质*/
	glVertexAttribPointer(T_VER, 2, GL_FLOAT, 0, 0, tex);
	glEnableVertexAttribArray(T_VER);

	/*从shader获取材质*/
	unis[0] = program.uniformLocation("tex_y");
	unis[1] = program.uniformLocation("tex_u");
	unis[2] = program.uniformLocation("tex_v");

	///*创建材质*/
	//glGenTextures(3, texs);
	////Y
	//glBindTexture(GL_TEXTURE_2D, texs[0]);
	///*放大缩小过滤，GL_LINEAR线性插值(GL_NEAREST插值效率高但马赛克严重)*/
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	///*创建材质显卡空间*/
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
	//
	////U
	//glBindTexture(GL_TEXTURE_2D, texs[1]);
	///*放大缩小过滤，线性插值*/
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	///*创建材质显卡空间*/
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

	////V
	//glBindTexture(GL_TEXTURE_2D, texs[2]);
	///*放大缩小过滤，线性插值*/
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	///*创建材质显卡空间*/
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

	///*分配材质内存空间*/
	//datas[0] = new unsigned char[width * height];	//Y
	//datas[1] = new unsigned char[width * height / 4];	//U
	//datas[2] = new unsigned char[width * height / 4];	//V
 
	/////*打开*/
	//fp = fopen("src_3840_2160.yuv", "rb");
	//if (!fp) {
	//	qDebug() << "src_3840_2160.yuv opened failed...";
	//}
	///*启动定时器*/
	//QTimer* ti = new QTimer(this);
	//connect(ti, SIGNAL(timeout()), this, SLOT(update()));
	//ti->start(40);
	return;
}

void XVideoWidget::resizeGL(int width, int height)
{
	std::lock_guard<std::mutex> lck(Gmtx_);
	return;
}
