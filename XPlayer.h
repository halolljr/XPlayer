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

	/// <summary>
	/// 返回内部ui界面的指针
	/// </summary>
	/// <returns>Ui::XPlayerClass *ui</returns>
	Ui::XPlayerClass* GetUI();
private:
	Ui::XPlayerClass *ui;
};
