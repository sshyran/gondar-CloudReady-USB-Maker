
#include "diskwritethread.h"

#include "deviceguy.h"

#include <QtWidgets>

extern "C" {
#include "gondar.h"
}

DiskWriteThread::DiskWriteThread(DeviceGuy * drive_in,
                                 QString* image_path_in,
                                 QObject *parent)
    : QThread(parent) {
    DeviceGuy_copy(drive_in, & selected_drive);
    image_path = * image_path_in;
}

DiskWriteThread::~DiskWriteThread() {
}

void DiskWriteThread::run() {
    qDebug() << "running diskwrite on image=" << image_path;
    const char * image_path_c_str = image_path.toStdString().c_str();
    Install(& selected_drive, image_path_c_str);
    qDebug() << "worker thread says complete";
    emit usbcomplete();
}
