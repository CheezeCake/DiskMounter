#include <unistd.h>

#include <QIcon>

#include "diskMounter.hpp"

DiskMounter::DiskMounter(QObject* parent) :
	QObject(parent), menu(new QMenu(nullptr)),
	sysTrayIcon(new QSystemTrayIcon(nullptr)),
	diskWatcher(DiskWatcher::getInstance())
{
	quitSeparatorAction = menu->addSeparator();
	QAction* quit = menu->addAction("Quit");
	QObject::connect(quit, SIGNAL(triggered(bool)), this,
			SLOT(onQuitClicked(bool)));

	sysTrayIcon->setContextMenu(menu.get());
	sysTrayIcon->setIcon(QIcon(":/images/hdd.png"));

	QObject::connect(&diskWatcher, SIGNAL(onDiskAppeared(QString, qint64)), this,
			SLOT(onDiskAppeared(QString, qint64)));
	QObject::connect(&diskWatcher, SIGNAL(onDiskDisappeared(QString)), this,
			SLOT(onDiskDisappeared(QString)));
}

void DiskMounter::run()
{
	diskWatcherThread = std::thread(&DiskMounter::runDiskWatcher, this);
	sysTrayIcon->show();
}

void DiskMounter::runDiskWatcher()
{
	diskWatcher.run();
}

void DiskMounter::onDiskClicked(QString deviceNodePath)
{
	QAction* action = devices.at(deviceNodePath);
	bool enabled = action->isEnabled();

	if (enabled) {
		pid_t child = fork();
		if (child == -1) {
			perror("fork");
			return;
		}

		if (child == 0) {
			QDir mountPoint = QDir::homePath();
			mountPoint.mkdir("./usb");
			mountPoint.cd("./usb");

			printf("mntPt absolute path : %s\n", mountPoint.absolutePath().toStdString().c_str());

			execl("/sbin/mount", "/sbin/mount", "-t", "msdos",
					deviceNodePath.toStdString().c_str(),
					mountPoint.absolutePath().toStdString().c_str(), nullptr);
			perror("/sbin/mount");
		}
	}

	action->setEnabled(!enabled);
}

void DiskMounter::onDiskAppeared(QString deviceNodePath, qint64 size)
{
	if (devices.find(deviceNodePath) == devices.end()) {
		QAction* deviceAction = menu->insertSection(quitSeparatorAction,
				createDiskLabel(deviceNodePath, size));
		QObject::connect(deviceAction, &QAction::triggered, [this, deviceNodePath] {
					onDiskClicked(deviceNodePath);
				});
		deviceAction->setSeparator(false);
		devices.emplace(deviceNodePath, deviceAction);
	}
}

void DiskMounter::onDiskDisappeared(QString deviceNodePath)
{
	const auto it = devices.find(deviceNodePath);
	if (it != devices.end()) {
		devices.erase(it);
		menu->removeAction(it->second);
	}
}

void DiskMounter::onQuitClicked(bool)
{
	diskWatcher.stop();
	diskWatcherThread.join();

	QApplication::quit();
}
