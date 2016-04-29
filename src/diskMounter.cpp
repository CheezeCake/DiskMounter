#include <iostream>

#include <sys/wait.h>
#include <unistd.h>

#include <Security/Authorization.h>

#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>

#include "diskMounter.hpp"

const char* DiskMounter::MountUtility = "/sbin/mount";
const char* DiskMounter::UmountUtility = "/sbin/umount";

DiskMounter::DiskMounter(QObject* parent) :
	QObject(parent), menu(new QMenu(nullptr)),
	sysTrayIcon(new QSystemTrayIcon(nullptr)),
	diskWatcher(DiskWatcher::getInstance())
{
	mountedSeparatorAction = menu->addSeparator();
	quitSeparatorAction = menu->addSeparator();
	QAction* quit = menu->addAction("Quit");
	QObject::connect(quit, SIGNAL(triggered(bool)), this,
			SLOT(onQuitClicked(bool)));

	sysTrayIcon->setContextMenu(menu.get());
	sysTrayIcon->setIcon(QIcon(":/images/hdd.png"));

	QObject::connect(&diskWatcher, SIGNAL(onDiskAppeared(QString, qint64)),
			this, SLOT(onDiskAppeared(QString, qint64)));
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

/* bool DiskMounter::execCmd(const char* cmd, const std::vector<char*>& cmdArgs) */
/* { */
/* 	const pid_t child = fork(); */

/* 	if (child == -1) { */
/* 		perror("fork"); */
/* 		return false; */
/* 	} */

/* 	if (child == 0) { */
/* 		execv(cmd, cmdArgs.data()); */
/* 		perror(cmd); */
/* 		exit(1); */
/* 	} */
/* 	else { */
/* 		int status; */
/* 		wait(&status); */

/* 		return (status == 0); */
/* 	} */
/* } */

bool DiskMounter::execCmd(const char* cmd, const std::vector<char*>& cmdArgs)
{
	AuthorizationRef authorizationRef;

	if (AuthorizationCreate(nullptr, kAuthorizationEmptyEnvironment,
			kAuthorizationFlagDefaults, &authorizationRef)
			!= errAuthorizationSuccess)
		return false;


	return (AuthorizationExecuteWithPrivileges(authorizationRef, cmd,
			kAuthorizationFlagDefaults, cmdArgs.data(), nullptr)
			== errAuthorizationSuccess);
}

void DiskMounter::runUtility(Device& device, const char* utility,
		const std::vector<char*>& execArgs, const QString& successMessage,
		const QString& errorMessage, bool mounted, const QString& mountPoint,
		QAction* before)
{
	// disable the menu action during the {,u}mount operation
	device.action->setEnabled(false);

	if (execCmd(utility, execArgs)) {
		QMessageBox::information(nullptr, utility, successMessage);

		// put the device in the correct section of the menu
		QString actionText = device.action->text();
		menu->removeAction(device.action);

		device.action = insertDiskAction(device.deviceNodePath, before,
				actionText);
		device.mounted = mounted;
		device.mountPoint = mountPoint;
	}
	else {
		QMessageBox::warning(nullptr, utility, errorMessage);

		device.action->setEnabled(true);
	}
}

void DiskMounter::mountDisk(Device& device)
{
	if (device.mounted) {
		std::cerr << device.deviceNodePath.toStdString() << " already mounted on : "
			<< device.mountPoint.toStdString() << '\n';
		return;
	}

	const QString fsType = QInputDialog::getText(nullptr, "fs type", "fs type",
			QLineEdit::Normal, "msdos");
	const QString mountPoint = QFileDialog::getExistingDirectory(nullptr,
			"mount point", QDir::homePath());
	const std::vector<char*> execArgs = {
		/* const_cast<char*>(MountUtility), */
		const_cast<char*>("-t"),
		const_cast<char*>(fsType.toStdString().c_str()),
		const_cast<char*>(device.deviceNodePath.toStdString().c_str()),
		const_cast<char*>(mountPoint.toStdString().c_str()),
		nullptr
	};

	runUtility(device, MountUtility, execArgs,
				device.deviceNodePath + " successfully mounted on " + mountPoint,
				device.deviceNodePath + " mount on " + mountPoint + " failed!",
				true, mountPoint, quitSeparatorAction);
}

void DiskMounter::unmountDisk(Device& device)
{
	if (!device.mounted) {
		std::cerr << device.deviceNodePath.toStdString() << " not mounted\n";
		return;
	}

	const std::vector<char*> execArgs = {
		/* const_cast<char*>(UmountUtility), */
		const_cast<char*>(device.mountPoint.toStdString().c_str()),
		nullptr
	};

	runUtility(device, UmountUtility, execArgs,
				device.deviceNodePath + " successfully unmounted from " +
				device.mountPoint,
				device.deviceNodePath + " unmount from " + device.mountPoint +
				" failed!",
				false, "", mountedSeparatorAction);
}

void DiskMounter::onDiskClicked(QString deviceNodePath)
{
	Device& device = devices.at(deviceNodePath);

	if (device.action->isEnabled()) {
		if (device.mounted) {
			if (QMessageBox::question(nullptr, "DiskMounter",
						"Unmount " + deviceNodePath + " ?",
						QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
				unmountDisk(device);
		}
		else {
			mountDisk(device);
		}
	}
}

QAction* DiskMounter::insertDiskAction(const QString& deviceNodePath,
		QAction* before, const QString& text)
{
	QAction* deviceAction = menu->insertSection(before, text);
	QObject::connect(deviceAction, &QAction::triggered, [this, deviceNodePath] {
			onDiskClicked(deviceNodePath);
			});
	deviceAction->setSeparator(false);

	return deviceAction;
}

void DiskMounter::onDiskAppeared(QString deviceNodePath, qint64 size)
{
	if (devices.find(deviceNodePath) == devices.end()) {
		devices.emplace(deviceNodePath, Device(deviceNodePath,
					insertDiskAction(deviceNodePath, mountedSeparatorAction,
						createDiskLabel(deviceNodePath, size))));
	}
}

void DiskMounter::onDiskDisappeared(QString deviceNodePath)
{
	const auto it = devices.find(deviceNodePath);
	if (it != devices.end()) {
		menu->removeAction(it->second.action);
		devices.erase(it);
	}
}

void DiskMounter::onQuitClicked(bool)
{
	diskWatcher.stop();
	diskWatcherThread.join();

	QApplication::quit();
}
