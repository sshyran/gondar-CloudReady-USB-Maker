// Copyright 2017 Neverware
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "gondarwizard.h"

#include <QNetworkReply>
#include <QProgressBar>
#include <QtWidgets>

#include "diskwritethread.h"
#include "downloader.h"
#include "unzipthread.h"

#include "device.h"
#include "gondar.h"
#include "log.h"
#include "metric.h"
#include "neverware_unzipper.h"

DeviceGuyList* drivelist = NULL;
DeviceGuy* selected_drive = NULL;

GondarButton::GondarButton(const QString& text,
                           unsigned int device_num,
                           QWidget* parent)
    : QRadioButton(text, parent) {
  index = device_num;
}
GondarWizard::GondarWizard(QWidget* parent)
    : QWizard(parent), about_shortcut_(QKeySequence::HelpContents, this) {
  // these pages are automatically cleaned up
  // new instances are made whenever navigation moves on to another page
  // according to qt docs
  setPage(Page_adminCheck, &adminCheckPage);
  // chromeoverLogin and imageSelect are alternatives to each other
  // that both progress to usbInsertPage
  setPage(Page_chromeoverLogin, &chromeoverLoginPage);
  setPage(Page_siteSelect, &siteSelectPage);
  setPage(Page_imageSelect, &imageSelectPage);
  setPage(Page_usbInsert, &usbInsertPage);
  setPage(Page_deviceSelect, &deviceSelectPage);
  setPage(Page_downloadProgress, &downloadProgressPage);
  setPage(Page_writeOperation, &writeOperationPage);
  setPage(Page_error, &errorPage);
  setWizardStyle(QWizard::ModernStyle);
  setWindowTitle(tr("Cloudready USB Creation Utility"));
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));

  setOption(QWizard::HaveCustomButton1, false);
  setOption(QWizard::NoCancelButton, true);
  setOption(QWizard::NoBackButtonOnLastPage, true);

  connect(&about_shortcut_, &QShortcut::activated, &about_dialog_,
          &gondar::AboutDialog::show);
  runTime = QDateTime::currentDateTime();
}

// handle event when 'make another usb' button pressed
void GondarWizard::handleMakeAnother() {
  // works as long as usbInsertPage is not the last page in wizard
  setStartId(usbInsertPage.nextId() - 1);
  restart();
}

int GondarWizard::nextId() const {
  if (errorPage.errorEmpty()) {
    return QWizard::nextId();
  } else {
    if (currentId() == Page_error) {
      return -1;
    } else {
      return Page_error;
    }
  }
}

void GondarWizard::postError(const QString& error) {
  QTimer::singleShot(0, this, [=]() { catchError(error); });
}

void GondarWizard::catchError(const QString& error) {
  LOG_ERROR << "displaying error: " << error;
  errorPage.setErrorString(error);
  next();
}

double GondarWizard::getRunTime() {
  double delta = QDateTime::currentDateTime().toMSecsSinceEpoch() -
                 runTime.toMSecsSinceEpoch();
  return delta;
}

UsbInsertPage::UsbInsertPage(QWidget* parent) : WizardPage(parent) {
  setTitle("Please insert an 8GB or 16GB USB storage device");
  setSubTitle(
      "In the next step, the selected device will be permanantly erased and "
      "turned into a CloudReady installer.");

  label.setText(
      "Sandisk devices are not recommended.  "
      "Devices with more than 16GB of space may be unreliable.  "
      "The next screen will become available once a valid "
      "destination drive is detected.");
  label.setWordWrap(true);

  layout.addWidget(&label);
  setLayout(&layout);

  // the next button should be grayed out until the user inserts a USB
  connect(this, SIGNAL(driveListRequested()), this, SLOT(getDriveList()));
}

void UsbInsertPage::initializePage() {
  tim = new QTimer(this);
  connect(tim, SIGNAL(timeout()), SLOT(getDriveList()));
  // if the page is visited again, delete the old drivelist
  if (drivelist != NULL) {
    DeviceGuyList_free(drivelist);
    drivelist = NULL;
  }
  // send a signal to check for drives
  emit driveListRequested();
}

bool UsbInsertPage::isComplete() const {
  // this should return false unless we have a non-empty result from
  // GetDevices()
  if (drivelist == NULL) {
    return false;
  } else {
    return true;
  }
}

void UsbInsertPage::getDriveList() {
  drivelist = GetDeviceList();
  if (DeviceGuyList_length(drivelist) == 0) {
    DeviceGuyList_free(drivelist);
    drivelist = NULL;
    tim->start(1000);
  } else {
    tim->stop();
    showDriveList();
  }
}

void UsbInsertPage::showDriveList() {
  emit completeChanged();
}

DeviceSelectPage::DeviceSelectPage(QWidget* parent) : WizardPage(parent) {
  // this page should just say 'hi how are you' while it stealthily loads
  // the usb device list.  or it could ask you to insert your device
  setTitle("USB device selection");
  setSubTitle("Choose your target device from the list of devices below.");
  layout = new QVBoxLayout;
  drivesLabel.setText("Select Drive:");
  radioGroup = NULL;
  setLayout(layout);
}

void DeviceSelectPage::initializePage() {
  // while our layout is not empty, remove items from it
  while (!layout->isEmpty()) {
    QLayoutItem* curItem = layout->takeAt(0);
    if (curItem->widget() != &drivesLabel) {
      delete curItem->widget();
    }
  }

  // remove our last listing
  delete radioGroup;

  if (drivelist == NULL) {
    return;
  }
  DeviceGuy* itr = drivelist->head;
  // Line up widgets horizontally
  // use QVBoxLayout for vertically, H for horizontal
  layout->addWidget(&drivesLabel);

  radioGroup = new QButtonGroup();
  // i could extend the button object to also have a secret index
  // then i could look up index later easily
  while (itr != NULL) {
    // FIXME(kendall): clean these up
    GondarButton* curRadio = new GondarButton(itr->name, itr->device_num, this);
    radioGroup->addButton(curRadio);
    layout->addWidget(curRadio);
    itr = itr->next;
  }
  setLayout(layout);
}

bool DeviceSelectPage::validatePage() {
  // TODO(kendall): check for NULL on bad cast
  GondarButton* selected =
      dynamic_cast<GondarButton*>(radioGroup->checkedButton());
  if (selected == NULL) {
    return false;
  } else {
    unsigned int selected_index = selected->index;
    selected_drive = DeviceGuyList_getByIndex(drivelist, selected_index);
    return true;
  }
}

int DeviceSelectPage::nextId() const {
  if (wizard()->downloadProgressPage.isComplete()) {
    return GondarWizard::Page_writeOperation;
  } else {
    return GondarWizard::Page_downloadProgress;
  }
}

WriteOperationPage::WriteOperationPage(QWidget* parent) : WizardPage(parent) {
  setTitle("Creating your CloudReady USB installer");
  setSubTitle("This process may take up to 20 minutes.");
  layout.addWidget(&progress);
  setLayout(&layout);
}

void WriteOperationPage::initializePage() {
  writeFinished = false;
  // what if we just start writing as soon as we get here
  if (selected_drive == NULL) {
    qDebug() << "ERROR: no drive selected";
  } else {
    writeToDrive();
  }
}

bool WriteOperationPage::isComplete() const {
  return writeFinished;
}

bool WriteOperationPage::validatePage() {
  return writeFinished;
}

void WriteOperationPage::writeToDrive() {
  qDebug() << "Writing to drive...";
  image_path.clear();
  image_path.append(wizard()->downloadProgressPage.getImageFileName());
  showProgress();
  diskWriteThread = new DiskWriteThread(selected_drive, image_path, this);
  connect(diskWriteThread, SIGNAL(finished()), this, SLOT(onDoneWriting()));
  qDebug() << "launching thread...";
  diskWriteThread->start();
}

void WriteOperationPage::showProgress() {
  progress.setRange(0, 0);
  progress.setValue(0);
}

void WriteOperationPage::onDoneWriting() {
  switch (diskWriteThread->state()) {
    case DiskWriteThread::State::Initial:
    case DiskWriteThread::State::Running:
      // It should not be possible to get here at runtime
      writeFailed("Internal state error");
      return;

    case DiskWriteThread::State::GetFileSizeFailed:
      writeFailed("Error reading the disk image's file size");
      return;

    case DiskWriteThread::State::InstallFailed:
      writeFailed("Error writing to the USB device");
      return;

    case DiskWriteThread::State::Success:
      // Hooray!
      break;
  }

  setTitle("CloudReady USB created!");
  setSubTitle("You may now either exit or create another USB.");
  qDebug() << "install call returned";
  writeFinished = true;
  progress.setRange(0, 100);
  progress.setValue(100);
  wizard()->setOption(QWizard::HaveCustomButton1, true);
  // when a USB was successfully created, report time the run took
  gondar::SendMetric(gondar::Metric::SuccessDuration,
                     QString::number(wizard()->getRunTime()).toStdString());
  emit completeChanged();
}

// though error page follows in index, this is the end of the wizard for
// healthy flows
int WriteOperationPage::nextId() const {
  return -1;
}

void WriteOperationPage::setVisible(bool visible) {
  WizardPage::setVisible(visible);
  GondarWizard* wiz = dynamic_cast<GondarWizard*>(wizard());
  if (visible) {
    setButtonText(QWizard::CustomButton1, "Make Another USB");
    connect(wiz, SIGNAL(customButtonClicked(int)), wiz,
            SLOT(handleMakeAnother()));
  } else {
    wiz->setOption(QWizard::HaveCustomButton1, false);
    disconnect(wiz, SIGNAL(customButtonClicked(int)), wiz,
               SLOT(handleMakeAnother()));
  }
}

void WriteOperationPage::writeFailed(const QString& errorMessage) {
  wizard()->postError(errorMessage);
  writeFinished = true;
  emit completeChanged();
}
