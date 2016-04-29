#include <iostream>

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/ucred.h>
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

bool DiskMounter::execCmd(const char* cmd, const std::vector<char*>& cmdArgs)
{
	AuthorizationRef authorizationRef;
	FILE* pipe;
	bool ret = false;

	if (AuthorizationCreate(nullptr, kAuthorizationEmptyEnvironment,
			kAuthorizationFlagDefaults, &authorizationRef)
			!= errAuthorizationSuccess)
		return false;

	if (AuthorizationExecuteWithPrivileges(authorizationRef, cmd,
			kAuthorizationFlagDefaults, cmdArgs.data(), &pipe)
			== errAuthorizationSuccess) {
		while (fgetc(pipe) != EOF)
			;

		ret = true;
	}

	fclose(pipe);
	return ret;
}

void DiskMounter::runUtility(Device& device, const char* utility,
		const std::vector<char*>& execArgs, const QString& successMessage,
		const QString& errorMessage, bool mounted, const QString& mountPoint,
		QAction* before, const std::function<bool()>& success)
{
	// disable the menu action during the {,u}mount operation
	device.action->setEnabled(false);

	if (execCmd(utility, execArgs) && success()) {
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

bool DiskMounter::isMounted(const char* deviceNodePath,
		const char* mountPoint)
{
	struct statfs* stfs;
	int n;
	if ((n = getmntinfo(&stfs, MNT_WAIT)) == 0)
		return false;

	for (int i = 0; i < n; i++) {
		if (strcmp(stfs[i].f_mntfromname, deviceNodePath) == 0 &&
				strcmp(stfs[i].f_mntonname, mountPoint) == 0)
			return true;
	}

	return false;
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
		const_cast<char*>("-t"),
		const_cast<char*>(fsType.toStdString().c_str()),
		const_cast<char*>(device.deviceNodePath.toStdString().c_str()),
		const_cast<char*>(mountPoint.toStdString().c_str()),
		nullptr
	};

	runUtility(device, MountUtility, execArgs,
				device.deviceNodePath + " successfully mounted on " + mountPoint,
				device.deviceNodePath + " mount on " + mountPoint + " failed!",
				true, mountPoint, quitSeparatorAction,
				[&device, &mountPoint] {
					return isMounted(
							device.deviceNodePath.toStdString().c_str(),
							mountPoint.toStdString().c_str());
				});
}

void DiskMounter::unmountDisk(Device& device)
{
	if (!device.mounted) {
		std::cerr << device.deviceNodePath.toStdString() << " not mounted\n";
		return;
	}

	const std::vector<char*> execArgs = {
		const_cast<char*>(device.mountPoint.toStdString().c_str()),
		nullptr
	};

	runUtility(device, UmountUtility, execArgs,
				device.deviceNodePath + " successfully unmounted from " +
				device.mountPoint,
				device.deviceNodePath + " unmount from " + device.mountPoint +
				" failed!",
				false, "", mountedSeparatorAction,
				[&device] {
					return !isMounted(
							device.deviceNodePath.toStdString().c_str(),
							device.mountPoint.toStdString().c_str());
				});
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
