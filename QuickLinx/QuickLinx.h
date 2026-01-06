#pragma once

#include <QtWidgets/QMainWindow>

#include "EthDriver.h"
#include "ui_QuickLinx.h"

/*
	File: QuickLinx.h

	Description:
		Main application window for QuickLinx. Inherits from QMainWindow and provides
		UI controls for importing, exporting, merging, and overwriting Ethernet driver
		configurations.
*/

class QuickLinx : public QMainWindow
{
	Q_OBJECT

public:
	// Constructor and destructor
	QuickLinx(QWidget *parent = nullptr);
	~QuickLinx();

	// Updates the progress bar with current step and total steps.
	// Automatically calculates and displays percentage (0-100%).
	void update_progress_bar(int current_step, int total_steps);

private slots:
	// Exports all AB_ETH drivers from the registry to a CSV file
	void on_export_button_clicked();

	// Imports AB_ETH drivers from a CSV file for staging
	void on_import_button_clicked();

	// Merges staged CSV drivers with existing registry drivers
	void on_merge_button_clicked();

	// Overwrites existing registry drivers with staged CSV drivers
	void on_overwrite_button_clicked();

private:
	// Qt designer-generated UI object
	Ui::QuickLinxClass ui;

	// Staged drivers loaded from CSV file, awaiting merge or overwrite
	std::vector<EthDriver> m_csv_drivers;

	// Registry drivers currently loaded (used for comparison/merge operations)
	std::vector<EthDriver> m_reg_drivers;
};

