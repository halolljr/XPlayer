#include "XPlayer.h"

XPlayer::XPlayer(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::XPlayerClass())
{
	ui->setupUi(this);
}

XPlayer::~XPlayer()
{
	delete ui;
}
