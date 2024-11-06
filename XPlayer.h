#pragma once

#include <QMainWindow>
#include "ui_XPlayer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class XPlayerClass; };
QT_END_NAMESPACE

class XPlayer : public QMainWindow
{
	Q_OBJECT

public:
	XPlayer(QWidget *parent = nullptr);
	~XPlayer();

private:
	Ui::XPlayerClass *ui;
};
