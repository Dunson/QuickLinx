#include "QuickLinx.h"

#include <QtWidgets/QApplication>

// Main entry point for the QuickLinx application.
// Initializes the Qt application, sets the window icon, and displays the main window.
int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	// Set the application window icon
	app.setWindowIcon(QIcon(":/icons/quicklinx.ico"));

	// Create and display the main application window
	QuickLinx window;
	window.show();

	// Start the Qt event loop
	return app.exec();
}
