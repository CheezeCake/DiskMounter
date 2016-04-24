#pragma once

#include <map>
#include <memory>
#include <thread>

#include <DiskArbitration/DiskArbitration.h>

#include <QtWidgets/QtWidgets>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>

#include "diskWatcher.hpp"

class DiskMounter : public QObject
{
	Q_OBJECT

	public:
		struct Device
		{
			QString deviceNodePath;
			QString mountPoint;
			QAction* action;
			bool mounted;

			Device(const QString& nodePath, QAction* act) :
				deviceNodePath(nodePath), action(act), mounted(false)
			{
			}
		};

		static const char* MountUtility;
		static const char* UmountUtility;

	private:
		std::unique_ptr<QMenu> menu;
		std::unique_ptr<QSystemTrayIcon> sysTrayIcon;

		DiskWatcher& diskWatcher;
		std::thread diskWatcherThread;
		QAction* mountedSeparatorAction;
		QAction* quitSeparatorAction;
		std::map<QString, Device> devices;

		void runDiskWatcher();
		void onDiskClicked(QString deviceNodePath);

		static inline QString createDiskLabel(QString deviceNodePath,
				qint64 size)
		{
			return (deviceNodePath + " (" +
					QString::number(size / (1024 * 1024 * 1024)) + " GiB)");
		}

		QAction* insertDiskAction(const QString& deviceNodePath,
				QAction* before, const QString& text);
		void runUtility(Device& device, const char* utility,
				const std::vector<char*>& execArgs,
				const QString& successMessage, const QString& errorMessage,
				bool mounted, const QString& mountPoint, QAction* before);

		bool execCmd(const char* cmd, const std::vector<char*>& cmdArgs);
		void mountDisk(Device& device);
		void unmountDisk(Device& device);

	private slots:
		void onQuitClicked(bool checked = false);
		void onDiskAppeared(QString deviceNodePath, qint64 size);
		void onDiskDisappeared(QString deviceNodePath);

	public:
		DiskMounter(QObject* parent = nullptr);

		void run();
};
