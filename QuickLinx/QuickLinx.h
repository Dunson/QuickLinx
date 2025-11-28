#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QuickLinx.h"

class QuickLinx : public QMainWindow
{
    Q_OBJECT

public:
    QuickLinx(QWidget *parent = nullptr);
    ~QuickLinx();

private:
    Ui::QuickLinxClass ui;
};

