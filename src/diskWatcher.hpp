#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <DiskArbitration/DiskArbitration.h>

#include <QObject>

class DiskWatcher : public QObject
{
	Q_OBJECT

	private:
		DiskWatcher(QObject* parent = nullptr);

		std::atomic<bool> done;

		static void onDiskAppearedCallback(DADiskRef disk, void* context);
		void onDiskAppearedCallbackEmit(DADiskRef disk);
		static void onDiskDisappearedCallback(DADiskRef disk, void* context);
		void onDiskDisappearedCallbackEmit(DADiskRef disk);

		static QString getDeviceNodePathFromDisk(DADiskRef disk);
		static qint64 getDeviceSizeFromDisk(DADiskRef disk);
		static bool isPartition(DADiskRef disk);

	public:
		static DiskWatcher& getInstance();
		void run();

		DiskWatcher(const DiskWatcher&) = delete;
		void operator=(const DiskWatcher&) = delete;
		void stop();

	signals:
		void onDiskAppeared(QString deviceNodePath, qint64 size);
		void onDiskDisappeared(QString deviceNodePath);
};
