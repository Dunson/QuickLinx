	#include "QuickLinx.h"
#include "RegistryManager.h"
#include "CSV.h"
#include "ImportEngine.h"

#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <windows.h>

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

// Debug Helper Functions (Anonymous Namespace)
namespace
{
	// Allocates a debug console window and redirects stdin/stdout/stderr to it.
	// Uses UTF-16 output mode for proper wide-character display.
	// Safe to call multiple times; only allocates once.
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

			// Set output mode to UTF-16 for proper wide-character display
			_setmode(_fileno(stdout), _O_U16TEXT);
		}
	}

	// Clears the debug console screen by executing the 'cls' system command.
	static void ClearConsole()
	{
		std::system("cls");
	}

	// Dumps a list of EthDriver structures to the debug console.
	// Displays driver metadata (name, key, station, node count) and all node IP addresses.
	// Automatically attaches the debug console if not already attached.
	static void DumpDriversToConsole(const std::vector<EthDriver>& drivers)
	{
		AttachConsole();

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

} // namespace

// Constructor: initializes the main application window.
// Sets up UI elements, progress bar, and button cursor styles.
QuickLinx::QuickLinx(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// Initialize UI element states
	ui.status_label->setText("Ready");
	ui.progress_bar->setValue(0);
	ui.merge_button->setEnabled(false);
	ui.overwrite_button->setEnabled(false);

	// Set all buttons to use a pointer cursor for better UX
	const auto buttons = findChildren<QPushButton*>();
	for (QPushButton* button : buttons)
	{
		button->setCursor(Qt::PointingHandCursor);
	}
}

// Destructor: cleanup is handled automatically by Qt's parent-child ownership.
QuickLinx::~QuickLinx()
{}

// Exports all AB_ETH drivers from the registry to a CSV file.
// User selects the save location; the operation is aborted if no drivers are found.
void QuickLinx::on_export_button_clicked()
{
	// Prompt user for save location
	QString default_path = QDir::homePath() + "/QuickLinx_Export.csv";

	QString file_name = QFileDialog::getSaveFileName(
		this,
		"Export QuickLinx Drivers to CSV",
		default_path,
		"CSV Files (*.csv);;All Files (*)");

	if (file_name.isEmpty())
	{
		return; // User cancelled the dialog
	}

	ui.status_label->setText("Exporting...");
	update_progress_bar(0, 1);

	// Load all drivers from the registry
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

	// Write drivers to CSV file
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

	// Success
	ui.status_label->setText("Export completed successfully.");
	update_progress_bar(1, 1);
}

// Imports AB_ETH drivers from a CSV file and stages them for merge or overwrite.
// Validates CSV format; enables merge/overwrite buttons upon successful import.
void QuickLinx::on_import_button_clicked()
{
	// Prompt user to select a CSV file
	QString file_name = QFileDialog::getOpenFileName(
		this,
		"Import QuickLinx Drivers from CSV",
		QDir::homePath(),
		"CSV Files (*.csv);;All Files (*)");

	if (file_name.isEmpty())
	{
		return; // User cancelled the dialog
	}

	ui.status_label->setText("Validating Format...");
	update_progress_bar(0, 1);

	// Parse CSV file into EthDriver structures
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

	// Successful import; stage drivers and enable action buttons
	QString summary = QString("Parsed %1 driver(s) from CSV. Ready for Import").arg(m_csv_drivers.size());
	QMessageBox::information(this, "Import Test OK", summary);

	ui.status_label->setText("Import Successful! Ready to Merge/Overwrite");
	update_progress_bar(1, 1);
	ui.merge_button->setEnabled(true);
	ui.overwrite_button->setEnabled(true);
}

// Merges staged CSV drivers with existing registry drivers.
// Combines node lists for matching drivers (by name); adds new drivers with generated keys.
// Displays a summary of changes and any errors encountered.
void QuickLinx::on_merge_button_clicked()
{
	// Verify that CSV drivers have been imported
	if (m_csv_drivers.empty())
	{
		QMessageBox::warning(
			this,
			"Merge Failed",
			"No imported CSV drivers available to merge. Please import a CSV file first.");
		return;
	}

	// Load existing drivers from registry
	auto registry_drivers = RegistryManager::LoadDrivers();
	if (registry_drivers.empty())
	{
		QMessageBox::warning(
			this,
			"Merge Failed",
			"No AB_ETH drivers found in the Registry to merge with.");
		return;
	}

	// Compute merge changes
	ui.status_label->setText("Merging Drivers...");
	update_progress_bar(0, 1);

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
		update_progress_bar(1, 1);
		return;
	}

	// Disable buttons during save operations
	ui.export_button->setEnabled(false);
	ui.import_button->setEnabled(false);
	ui.merge_button->setEnabled(false);
	ui.overwrite_button->setEnabled(false);

	// Save updated and new drivers, tracking progress and errors
	bool all_ok = true;
	std::wstring save_errors;
	std::size_t saved_count = 0;

	auto save_driver_with_progress = [&](const EthDriver& drv)
	{
		if (!RegistryManager::SaveDriver(drv))
		{
			all_ok = false;
			save_errors += L"Failed to save driver '" + drv.name + L"' (" + drv.key_name + L")\n";
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

	// Re-enable import/export buttons
	ui.export_button->setEnabled(true);
	ui.import_button->setEnabled(true);

	// Build error/warning summary
	QString details;

	if (!result.errors.empty())
	{
		for (const auto& e : result.errors)
			details += QString::fromStdWString(e) + "\n";
	}
	if (!save_errors.empty())
	{
		details += QString::fromStdWString(save_errors) + "\n";
	}

	// Display results
	if (!details.isEmpty())
	{
		ui.status_label->setText("Merge completed with errors.");
		QMessageBox::warning(
			this,
			"Merge Completed with Errors",
			QString("The merge operation completed, but some errors occurred:\n\n%1").arg(details));
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

	// Clear staged CSV drivers
	m_csv_drivers.clear();
}

// Overwrites existing registry drivers with staged CSV drivers.
// Matching drivers (by name) have their node lists completely replaced.
// New drivers are added with generated keys. Displays a summary of changes and errors.
void QuickLinx::on_overwrite_button_clicked()
{
	// Verify that CSV drivers have been imported
	if (m_csv_drivers.empty())
	{
		QMessageBox::warning(
			this,
			"Overwrite Failed",
			"No imported CSV drivers available to overwrite. "
			"Please import a CSV file first.");
		return;
	}

	// Load existing drivers from registry
	auto registry_drivers = RegistryManager::LoadDrivers();
	if (registry_drivers.empty())
	{
		QMessageBox::warning(
			this,
			"Overwrite Failed",
			"No existing AB_ETH drivers were found in the registry.");
		return;
	}

	// Compute overwrite changes
	ui.status_label->setText("Overwriting drivers...");
	update_progress_bar(0, 1);

	ImportEngine::ImportResult result =
		ImportEngine::overwrite_drivers(registry_drivers, m_csv_drivers);

	const std::size_t total_ops =
		result.updated_drivers.size() + result.new_drivers.size();

	if (total_ops == 0)
	{
		ui.status_label->setText("Overwrite complete. No changes needed.");
		update_progress_bar(1, 1);
		return;
	}

	// Disable buttons during save operations
	ui.export_button->setEnabled(false);
	ui.import_button->setEnabled(false);
	ui.merge_button->setEnabled(false);
	ui.overwrite_button->setEnabled(false);

	// Save updated and new drivers, tracking progress and errors
	std::size_t completed = 0;
	auto bump_progress = [&]()
	{
		++completed;
		update_progress_bar(static_cast<int>(completed), static_cast<int>(total_ops));
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

	// Re-enable import/export buttons
	ui.export_button->setEnabled(true);
	ui.import_button->setEnabled(true);

	// Clear staged CSV drivers
	m_csv_drivers.clear();

	// Build error/warning summary
	QString details;
	for (const auto& e : result.errors)
		details += QString::fromStdWString(e) + "\n";
	if (!save_errors.empty())
		details += QString::fromStdWString(save_errors);

	// Display results
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
}

// Updates the progress bar with the specified progress ratio.
// Calculates and displays a percentage (0-100%) and refreshes the UI.
void QuickLinx::update_progress_bar(int current_step, int total_steps)
{
	if (total_steps <= 0)
		total_steps = 1; // Prevent division by zero

	int percent = static_cast<int>(
		100.0 * static_cast<double>(current_step) / static_cast<double>(total_steps));

	// Clamp percentage to valid range
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;

	ui.progress_bar->setValue(percent);

	// Process pending UI events to keep the UI responsive
	qApp->processEvents();
}




