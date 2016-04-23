#include <DiskArbitration/DASession.h>
#include "diskWatcher.hpp"

DiskWatcher& DiskWatcher::getInstance()
{
	static DiskWatcher instance;
	return instance;
}

QString DiskWatcher::getDeviceNodePathFromDisk(DADiskRef disk)
{
	const QString dev("/dev/");
	const QString BSDName(DADiskGetBSDName(disk));
	return (dev + BSDName);
}

qint64 DiskWatcher::getDeviceSizeFromDisk(DADiskRef disk)
{
	CFDictionaryRef dict = DADiskCopyDescription(disk);
	const void* value = CFDictionaryGetValue(dict,
			kDADiskDescriptionMediaSizeKey);
	qint64 ret;
	CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &ret);
	CFRelease(dict);

	return ret;
}

bool DiskWatcher::isPartition(DADiskRef disk)
{
	CFDictionaryRef dict = DADiskCopyDescription(disk);
	const void* value = CFDictionaryGetValue(dict,
			kDADiskDescriptionMediaLeafKey);
	bool ret = CFBooleanGetValue((CFBooleanRef)value);
	CFRelease(dict);

	return ret;
}

DiskWatcher::DiskWatcher(QObject* parent) :
	QObject(parent), done(false)
{
}

void DiskWatcher::run()
{
	DASessionRef session = DASessionCreate(kCFAllocatorDefault);
	DARegisterDiskAppearedCallback(session,
			kDADiskDescriptionMatchVolumeUnrecognized, onDiskAppearedCallback,
			nullptr);
	DARegisterDiskDisappearedCallback(session,
			kDADiskDescriptionMatchVolumeUnrecognized,
			onDiskDisappearedCallback, nullptr);

	DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(),
			kCFRunLoopDefaultMode);

	while (!done)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, true);

	DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(),
			kCFRunLoopDefaultMode);
	CFRelease(session);
}

void DiskWatcher::stop()
{
	done = true;
}

void DiskWatcher::onDiskAppearedCallback(DADiskRef disk, void*)
{
	if (isPartition(disk))
			getInstance().onDiskAppearedCallbackEmit(disk);
}

void DiskWatcher::onDiskAppearedCallbackEmit(DADiskRef disk)
{
	emit onDiskAppeared(getDeviceNodePathFromDisk(disk),
			getDeviceSizeFromDisk(disk));
}

void DiskWatcher::onDiskDisappearedCallback(DADiskRef disk, void*)
{
	if (isPartition(disk))
			getInstance().onDiskDisappearedCallbackEmit(disk);
}

void DiskWatcher::onDiskDisappearedCallbackEmit(DADiskRef disk)
{
	emit onDiskDisappeared(getDeviceNodePathFromDisk(disk));
}
