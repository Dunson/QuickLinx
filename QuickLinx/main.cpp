#include "QuickLinx.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
	app.setWindowIcon(QIcon(":/icons/quicklinx.ico"));
    QuickLinx window;
    window.show();
    return app.exec();
}
