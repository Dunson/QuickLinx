#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QuickLinx.h"

class QuickLinx : public QMainWindow
{
    Q_OBJECT

public:
    QuickLinx(QWidget *parent = nullptr);
    ~QuickLinx();

    void DebugPrintDriversToConsole();
    void UpdateProgress(int current_step, int total_steps);

private slots:
    void on_export_button_clicked();
    void on_import_button_clicked();
    void on_merge_button_clicked();
	void on_overwrite_button_clicked();

private:
    Ui::QuickLinxClass ui;
};

