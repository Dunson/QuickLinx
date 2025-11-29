#include "QuickLinx.h"
#include "RegistryManager.h"
#include "CSV.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

static void AttachConsole()
{
    static bool attached = false;
    if (attached)
        return;

    if (AllocConsole())
    {
        attached = true;

        // Redirect stdin/stdout/stderr to the new console
        FILE* fp;
        freopen_s(&fp, "CONIN$", "r", stdin);
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);

        // Make std::wcout use UTF-16 so your std::wstring prints nicely
        _setmode(_fileno(stdout), _O_U16TEXT);
    }
}

static void ClearConsole() { std::system("cls"); }

static void DumpDriversToDebug(const std::vector<EthDriver>& drivers)
{
    AttachConsole();
	ClearConsole();

    qDebug().noquote() << "========== Parsed EthDriver list ==========";

    for (const auto& d : drivers)
    {
        qDebug().noquote()
            << "Driver:" << QString::fromStdWString(d.name)
            << "| key_name:" << QString::fromStdWString(d.key_name)
            << "| station:" << d.station
            << "| nodes:" << d.nodes.size();

        for (size_t i = 0; i < d.nodes.size(); ++i)
        {
            qDebug().noquote()
                << "   [" << i << "]"
                << QString::fromStdWString(d.nodes[i]);
        }
    }

    qDebug().noquote() << "===========================================";
}

QuickLinx::QuickLinx(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

	// Set initial status for some UI elements
	ui.status_label->setText("Ready");
	ui.progress_bar->setValue(0);
	ui.merge_button->setEnabled(false);
	ui.overwrite_button->setEnabled(false);

	// Set all button to use a pointer cursor
	const auto buttons = findChildren<QPushButton*>();
	for (QPushButton* button : buttons) {
		button->setCursor(Qt::PointingHandCursor);
	}

}

QuickLinx::~QuickLinx()
{}

// Button Event Handlers
void QuickLinx::on_export_button_clicked()
{
	// 1. Ask user where to save the CSV file.
	QString default_path = QDir::homePath() + "/QuickLinx_Export.csv";

    QString file_name = QFileDialog::getSaveFileName(
        this,
        "Export QuickLinx Drivers to CSV",
        default_path,
		"CSV Files (*.csv);;All Files (*)");

    if (file_name.isEmpty())
    {
        return;
    }

    ui.status_label->setText("Exporting...");
    UpdateProgress(0, 1);

	// 2. Load drivers from registry.
	std::vector<EthDriver> drivers = RegistryManager::LoadDrivers();
    if (drivers.empty())
    {
        QMessageBox::warning(
            this,
            "Export Failed",
			"No AB_ETH drivers found in the Registry to export.");
		ui.status_label->setText("Export failed: No drivers found.");
        return;
    }

	DumpDriversToDebug(drivers);

	// 3. Export to CSV.
    std::wstring error;
    if (!CSV::WriteDriversToFile(file_name.toStdWString(), drivers, error))
    {
        QMessageBox::critical(
            this,
            "Export Failed",
            QString::fromStdWString(error));

		ui.status_label->setText("Export failed.");
        UpdateProgress(0, 1);
		ui.progress_bar->setValue(0);
        return;
    }

    // 4. Update UI status.
	ui.status_label->setText("Export completed successfully.");
    UpdateProgress(1, 1);
}

void QuickLinx::on_import_button_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(
        this, 
        "Import QuickLinx Drivers from CSV",
        QDir::homePath(),
		"CSV Files (*.csv);;All Files (*)");

    if (file_name.isEmpty())
        return;

	ui.status_label->setText("Validating Format...");
    UpdateProgress(0, 1);

	std::wstring error;
    std::vector<EthDriver> drivers;

    if (!CSV::ReadDriversFromFile(file_name.toStdWString(), drivers, error))
    {
        QMessageBox::critical(
            this,
            "Import Failed",
            QString::fromWCharArray(error.c_str()));

		ui.status_label->setText("Import failed. CSV error.");
        UpdateProgress(0, 1);
        return;
    }

	DumpDriversToDebug(drivers);

	// Successful read - Dont touch Registry yet.
    QString summary = QString("Parsed %1 driver(s) from CSV.").arg(drivers.size());

    QMessageBox::information(this, "Import Test OK", summary);
	ui.status_label->setText("Import Successful! (Parse Test Only)");
    UpdateProgress(1, 1);
}

void QuickLinx::on_merge_button_clicked()
{
}

void QuickLinx::on_overwrite_button_clicked()
{
}

// Update the progress bar
void QuickLinx::UpdateProgress(int current_step, int total_steps)
{
    if (total_steps <= 0)
		total_steps = 1; // prevent division by zero

    int percent = static_cast<int>(
		100.0 * static_cast<double>(current_step) / static_cast<double>(total_steps));

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    ui.progress_bar->setValue(percent);
    
    qApp->processEvents();

}




