#include <QApplication>
#include <QMessageBox>

#include "diskMounter.hpp"

int main(int argc, char** argv)
{
	Q_INIT_RESOURCE(DiskMounter);

	QApplication app(argc, argv);

	if (!QSystemTrayIcon::isSystemTrayAvailable()) {
		QMessageBox::critical(0, QObject::tr("Systray"),
				QObject::tr("I couldn't detect any system tray "
					"on this system."));
		return 1;
	}

	QApplication::setQuitOnLastWindowClosed(false);

	DiskMounter diskMounter;
	diskMounter.run();

	return app.exec();
}
