!macx {
	error(DiskMounter is only compatible with OS X)
}

CONFIG += c++11

QT += widgets

RESOURCES = DiskMounter.qrc

SOURCES += src/main.cpp src/diskMounter.cpp src/diskWatcher.cpp

HEADERS += src/diskMounter.hpp src/diskWatcher.hpp

LIBS += -framework CoreFoundation -framework DiskArbitration

QMAKE_INFO_PLIST = plist/Info.plist
