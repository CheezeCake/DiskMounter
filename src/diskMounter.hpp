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

	private:
		std::unique_ptr<QMenu> menu;
		std::unique_ptr<QSystemTrayIcon> sysTrayIcon;

		DiskWatcher& diskWatcher;
		std::thread diskWatcherThread;
		QAction* quitSeparatorAction;
		std::map<QString, QAction*> devices;

		void runDiskWatcher();
		void onDiskClicked(QString deviceNodePath);

		static inline QString createDiskLabel(QString deviceNodePath, qint64 size)
		{
			return (deviceNodePath + " (" + QString::number(size / (1024 * 1024 * 1024)) + " GiB)");
		}

	private slots:
		void onQuitClicked(bool checked = false);
		void onDiskAppeared(QString deviceNodePath, qint64 size);
		void onDiskDisappeared(QString deviceNodePath);

	public:
		DiskMounter(QObject* parent = nullptr);

		void run();
};
