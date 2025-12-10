#include "QuickLinx.h"
#include "RegistryManager.h"
#include "CSV.h"
#include "ImportEngine.h"

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

static void DumpDriversToConsole(const std::vector<EthDriver>& drivers)
{
    AttachConsole();
	//ClearConsole();

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
    update_progress_bar(0, 1);

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

	DumpDriversToConsole(drivers);

	// 3. Export to CSV.
    std::wstring error;
    if (!CSV::write_drivers_to_file(file_name.toStdWString(), drivers, error))
    {
        QMessageBox::critical(
            this,
            "Export Failed",
            QString::fromStdWString(error));

		ui.status_label->setText("Export failed.");
        update_progress_bar(0, 1);
		ui.progress_bar->setValue(0);
        return;
    }

    // 4. Update UI status.
	ui.status_label->setText("Export completed successfully.");
    update_progress_bar(1, 1);
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
    update_progress_bar(0, 1);

	std::wstring error;

    if (!CSV::read_drivers_from_file(file_name.toStdWString(), m_csv_drivers, error))
    {
        QMessageBox::critical(
            this,
            "Import Failed",
            QString::fromWCharArray(error.c_str()));

		ui.status_label->setText("Import failed. CSV error.");
        update_progress_bar(0, 1);
        return;
    }

	//DumpDriversToConsole(drivers);

	// Successful read - Dont touch Registry yet.
    QString summary = QString("Parsed %1 driver(s) from CSV. Ready for Import").arg(m_csv_drivers.size());

    QMessageBox::information(this, "Import Test OK", summary);
	ui.status_label->setText("Import Successful! Ready to Merge/Overwrite");
    update_progress_bar(1, 1);
	ui.merge_button->setEnabled(true);
    ui.overwrite_button->setEnabled(true);
}

void QuickLinx::on_merge_button_clicked()
{
    // Make sure have drivers staged for merging
    if (m_csv_drivers.empty())
    {
        QMessageBox::warning(
            this,
            "Merge Failed",
            "No imported CSV drivers available to merge_drivers. Please import a CSV file first.");
        return;
    }

	// Load existing drivers from registry
	auto registry_drivers = RegistryManager::LoadDrivers();
    if (registry_drivers.empty())
    {
        QMessageBox::warning(
            this,
            "Merge Failed",
			"No AB_ETH drivers found in the Registry to merge_drivers with.");
		return;
    }

    // Let the engine compute what to change/add
	ui.status_label->setText("Merging Drivers...");
    update_progress_bar(0, 1); // 0%

    ImportEngine::ImportResult result =
		ImportEngine::merge_drivers(registry_drivers, m_csv_drivers);

    const std::size_t total_to_save =
		result.updated_drivers.size() + result.new_drivers.size();
    
    if (total_to_save == 0)
    {
        QMessageBox::information(
            this,
            "Merge Complete",
            "No changes were necessary. The Registry is already up to date.");
		update_progress_bar(1, 1); // 100%
        return;
    }
	
	//Disable Buttons while working
	ui.export_button->setEnabled(false);
	ui.import_button->setEnabled(false);
	ui.merge_button->setEnabled(false);
	ui.overwrite_button->setEnabled(false);

    // Save all drivers and update progress bar
	bool all_ok = true;
    std::wstring save_errors;

	std::size_t saved_count = 0;
    auto save_driver_with_progress = [&](const EthDriver& drv)
        {
            if (!RegistryManager::SaveDriver(drv)) 
            {
				all_ok = false;
				save_errors += L"Failed to save driver: " + drv.name + L"' (" + drv.key_name + L")\n";
            }
			++saved_count;
			update_progress_bar(static_cast<int>(saved_count), static_cast<int>(total_to_save));
        };

    // Save modified drivers
    for (const auto& d : result.updated_drivers)
        save_driver_with_progress(d);

    // Save new drivers
    for (const auto& d : result.new_drivers)
        save_driver_with_progress(d);

    // Re-enable buttons
    ui.export_button->setEnabled(true);
    ui.import_button->setEnabled(true);

    // Deal with any errors returned
    QString details;

    if (!result.errors.empty())
    {
        for(const auto& e : result.errors)
			details += QString::fromStdWString(e) + "\n";
    }
    if (!save_errors.empty())
    {
        details += QString::fromStdWString(save_errors) + "\n";
	}
    if (!details.isEmpty())
    {
		ui.status_label->setText("Merge completed with errors.");
        QMessageBox::warning(
            this,
            "Merge Completed with Errors",
			QString("The merge_drivers operation completed, but some errors occurred:\n\n%1").arg(details));
    }
    else if (!all_ok)
    {
		ui.status_label->setText("Merge failed while saving drivers.");
        QMessageBox::critical(
            this,
            "Merge Failed",
			"One or more drivers could not be saved to the Registry.");
    }
    else 
    {
		ui.status_label->setText("Merge completed successfully. Ready");
    }

    m_csv_drivers.clear();
}

void QuickLinx::on_overwrite_button_clicked()
{
    if (m_csv_drivers.empty())
    {
        QMessageBox::warning(
            this,
            "Overwrite Failed",
            "No imported CSV drivers available to overwrite. "
            "Please import a CSV file first.");
        return;
    }

    auto registry_drivers = RegistryManager::LoadDrivers();
    if (registry_drivers.empty())
    {
        QMessageBox::warning(
            this,
            "Overwrite Failed",
            "No existing AB_ETH drivers were found in the registry.");
        return;
    }

    ui.status_label->setText("Overwriting drivers...");
    update_progress_bar(0, 1);

    ImportEngine::ImportResult result =
        ImportEngine::overwrite_drivers(registry_drivers, m_csv_drivers);

    const std::size_t total_ops =
        result.updated_drivers.size() +
        result.new_drivers.size();

    if (total_ops == 0)
    {
        ui.status_label->setText("Overwrite complete. No changes needed.");
        update_progress_bar(1, 1);
        return;
    }

    ui.export_button->setEnabled(false);
    ui.import_button->setEnabled(false);
    ui.merge_button->setEnabled(false);
    ui.overwrite_button->setEnabled(false);

    std::size_t completed = 0;
    auto bump_progress = [&]()
        {
            ++completed;
            update_progress_bar(static_cast<int>(completed),
                static_cast<int>(total_ops));
        };

    bool all_ok = true;
    std::wstring save_errors;

    // Save updated existing drivers
    for (const auto& d : result.updated_drivers)
    {
        if (!RegistryManager::SaveDriver(d))
        {
            all_ok = false;
            save_errors += L"Failed to save driver '" + d.name +
                L"' (" + d.key_name + L")\n";
        }
        bump_progress();
    }

    // Save new drivers
    for (const auto& d : result.new_drivers)
    {
        if (!RegistryManager::SaveDriver(d))
        {
            all_ok = false;
            save_errors += L"Failed to save new driver '" + d.name +
                L"' (" + d.key_name + L")\n";
        }
        bump_progress();
    }

    ui.export_button->setEnabled(true);
    ui.import_button->setEnabled(true);

    // Clear staged CSV so the user must import again
    m_csv_drivers.clear();

    // Final status / errors
    QString details;
    for (const auto& e : result.errors)
        details += QString::fromStdWString(e) + "\n";
    if (!save_errors.empty())
        details += QString::fromStdWString(save_errors);

    if (!details.isEmpty())
    {
        ui.status_label->setText("Overwrite completed with issues.");
        QMessageBox::warning(
            this,
            "Overwrite Completed With Warnings",
            details);
    }
    else if (!all_ok)
    {
        ui.status_label->setText("Overwrite failed while saving drivers.");
        QMessageBox::critical(
            this,
            "Overwrite Failed",
            "One or more drivers could not be written to the registry.");
    }
    else
    {
        ui.status_label->setText("Overwrite completed successfully. Ready");
    }
	m_csv_drivers.clear();
}

// Update the progress bar
void QuickLinx::update_progress_bar(int current_step, int total_steps)
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




