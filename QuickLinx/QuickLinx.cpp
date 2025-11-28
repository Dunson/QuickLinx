#include "QuickLinx.h"

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

