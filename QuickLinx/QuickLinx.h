#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QuickLinx.h"

#include "EthDriver.h"

class QuickLinx : public QMainWindow
{
    Q_OBJECT

public:
    QuickLinx(QWidget *parent = nullptr);
    ~QuickLinx();

    void update_progress_bar(int current_step, int total_steps);

private slots:
    void on_export_button_clicked();
    void on_import_button_clicked();
    void on_merge_button_clicked();
	void on_overwrite_button_clicked();

private:
    Ui::QuickLinxClass ui;
	std::vector<EthDriver> m_csv_drivers;
};

