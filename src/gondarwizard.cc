
#include <QtWidgets>
#include <QNetworkReply>
#include <QProgressBar>

#include "gondarwizard.h"
#include "downloader.h"
#include "unzipthread.h"
#include "diskwritethread.h"

#include "gondar.h"
#include "deviceguy.h"
#include "neverware_unzipper.h"

DeviceGuyList* drivelist = NULL;
DeviceGuy* selected_drive = NULL;

GondarButton::GondarButton(const QString& text,
                           unsigned int device_num,
                           QWidget* parent)
    : QRadioButton(text, parent) {
  index = device_num;
}
GondarWizard::GondarWizard(QWidget* parent) : QWizard(parent) {
  // these pages are automatically cleaned up
  // new instances are made whenever navigation moves on to another page
  // according to qt docs
  setPage(Page_adminCheck, &adminCheckPage);
  setPage(Page_imageSelect, &imageSelectPage);
  setPage(Page_usbInsert, &usbInsertPage);
  setPage(Page_deviceSelect, &deviceSelectPage);
  setPage(Page_downloadProgress, &downloadProgressPage);
  setPage(Page_writeOperation, &writeOperationPage);
  setWizardStyle(QWizard::ModernStyle);
  setWindowTitle(tr("Cloudready USB Creation Utility"));

  // Only show next buttons for all screens
  QList<QWizard::WizardButton> button_layout;
  button_layout << QWizard::NextButton;
  setButtonLayout(button_layout);

  // initialize bitness selected
  bitnessSelected = NULL;
  setButtonText(QWizard::CustomButton1, tr("Make Another USB"));
  connect(this, SIGNAL(customButtonClicked(int)), this,
          SLOT(handleMakeAnother()));
}

// handle event when 'make another usb' button pressed
void GondarWizard::handleMakeAnother() {
  // we set the page to usbInsertPage and show usual buttons
  showUsualButtons();
  // works as long as usbInsertPage is not the last page in wizard
  setStartId(usbInsertPage.nextId() - 1);
  restart();
}
void GondarWizard::showUsualButtons() {
  // Only show next buttons for all screens
  QList<QWizard::WizardButton> button_layout;
  button_layout << QWizard::NextButton;
  setOption(QWizard::HaveCustomButton1, false);
  setButtonLayout(button_layout);
}

// show 'make another usb' button along with finish button at end of wizard
void GondarWizard::showFinishButtons() {
  QList<QWizard::WizardButton> button_layout;
  button_layout << QWizard::FinishButton;
  button_layout << QWizard::CustomButton1;
  setOption(QWizard::HaveCustomButton1, true);
  setButtonLayout(button_layout);
}

// note that even though this is called the admin check page, it will in most
// cases be a welcome page, unless the user is missing admin rights
AdminCheckPage::AdminCheckPage(QWidget* parent) : QWizardPage(parent) {
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));

  is_admin = false;  // assume false until we discover otherwise.
                     // this holds the user at this screen

  layout.addWidget(&label);
  setLayout(&layout);
}

void AdminCheckPage::initializePage() {
  is_admin = IsCurrentProcessElevated();
  if (!is_admin) {
    showIsNotAdmin();
  } else {
    showIsAdmin();
  }
}

bool AdminCheckPage::isComplete() const {
  // the next button is grayed out if user does not have appropriate rights
  return is_admin;
}

void AdminCheckPage::showIsAdmin() {
  setTitle("Welcome to the CloudReady USB Creation Utility");
  // note that a subtitle must be set  on a page in order for logo to display
  setSubTitle(
      "This utility will create a USB device that can be used to install "
      "CloudReady on any computer.");
  label.setText(
      "<p>You will need:</p><ul><li>8GB or 16GB USB stick</li><li>20 minutes "
      "for USB installer creation</li></ul>");
  label.setWordWrap(true);
  emit completeChanged();
}

void AdminCheckPage::showIsNotAdmin() {
  setTitle("User does not have administrator rights");
  setSubTitle(
      "The current user does not have adminstrator rights or the program was "
      "run without sufficient rights to create a USB.  Please re-run the "
      "program with sufficient rights.");
}

ImageSelectPage::ImageSelectPage(QWidget* parent) : QWizardPage(parent) {
  setTitle("Which version of CloudReady do you need?");
  setSubTitle(
      "64-bit should be suitable for most computers made after 2007.  Choose "
      "32-bit for older computers or devices with Intel Atom CPUs.");
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));
  thirtyTwo.setText("32-bit");
  sixtyFour.setText("64-bit (recommended)");
  sixtyFour.setChecked(true);
  bitnessButtons.addButton(&thirtyTwo);
  bitnessButtons.addButton(&sixtyFour);
  layout.addWidget(&thirtyTwo);
  layout.addWidget(&sixtyFour);
  setLayout(&layout);
  thirtyTwoUrl.setUrl(
      "https://ddnynf025unax.cloudfront.net/cloudready-free-56.3.80-32-bit/"
      "cloudready-free-56.3.80-32-bit.bin.zip");
  sixtyFourUrl.setUrl(
      "https://ddnynf025unax.cloudfront.net/cloudready-free-56.3.82-64-bit/"
      "cloudready-free-56.3.82-64-bit.bin.zip");
}

QUrl ImageSelectPage::getUrl() {
  QAbstractButton* selected = bitnessButtons.checkedButton();
  if (selected == &thirtyTwo) {
    return thirtyTwoUrl;
  } else if (selected == &sixtyFour) {
    return sixtyFourUrl;
  } else {
    // TODO: decide what this behavior should be
    return sixtyFourUrl;
  }
}

DownloadProgressPage::DownloadProgressPage(QWidget* parent)
    : QWizardPage(parent) {
  setTitle("CloudReady Download");
  setSubTitle("Your installer is currently downloading.");
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));
  download_finished = false;
  layout.addWidget(&progress);
  setLayout(&layout);
  range_set = false;
}

void DownloadProgressPage::initializePage() {
  setLayout(&layout);
  GondarWizard* wiz = dynamic_cast<GondarWizard*>(wizard());
  url = wiz->imageSelectPage.getUrl();
  qDebug() << "using url= " << url;
  QObject::connect(&manager, SIGNAL(finished()), this, SLOT(markComplete()));
  manager.append(url.toString());
  QObject::connect(&manager, SIGNAL(started()), this,
                   SLOT(onDownloadStarted()));
}

void DownloadProgressPage::onDownloadStarted() {
  QNetworkReply* cur_download = manager.getCurrentDownload();
  QObject::connect(cur_download, &QNetworkReply::downloadProgress, this,
                   &DownloadProgressPage::downloadProgress);
}

void DownloadProgressPage::downloadProgress(qint64 sofar, qint64 total) {
  if (!range_set) {
    range_set = true;
    progress.setRange(0, total);
  }
  progress.setValue(sofar);
}

void DownloadProgressPage::markComplete() {
  download_finished = true;
  // now that the download is finished, let's unzip the build.
  notifyUnzip();
  unzipThread = new UnzipThread(&url, this);
  connect(unzipThread, SIGNAL(finished()), this, SLOT(onUnzipFinished()));
  unzipThread->start();
}

void DownloadProgressPage::onUnzipFinished() {
  // unzip has now completed
  qDebug() << "main thread has accepted complete";
  progress.setRange(0, 100);
  progress.setValue(100);
  setSubTitle("Download and extraction complete!");
  emit completeChanged();
  // immediately progress to writeOperationPage
  wizard()->next();
}
void DownloadProgressPage::notifyUnzip() {
  setSubTitle("Extracting compressed image...");
  // setting range and value to zero results in an 'infinite' progress bar
  progress.setRange(0, 0);
  progress.setValue(0);
}

bool DownloadProgressPage::isComplete() const {
  return download_finished;
}

UsbInsertPage::UsbInsertPage(QWidget* parent) : QWizardPage(parent) {
  setTitle("Please insert an 8GB or 16GB USB storage device");
  setSubTitle(
      "In the next step, the selected device will be permanantly erased and "
      "turned into a CloudReady installer.");
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));

  label.setText(
      "Sandisk devices are not recommended.  "
      "Devices with more than 16GB of space may be unreliable.  "
      "The next screen will become available once a valid "
      "destination drive is detected.");
  label.setWordWrap(true);

  layout.addWidget(&label);
  setLayout(&layout);

  // the next button should be grayed out until the user inserts a USB
  QObject::connect(this, SIGNAL(driveListRequested()), this,
                   SLOT(getDriveList()));
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

DeviceSelectPage::DeviceSelectPage(QWidget* parent) : QWizardPage(parent) {
  // this page should just say 'hi how are you' while it stealthily loads
  // the usb device list.  or it could ask you to insert your device
  setTitle("USB device selection");
  setSubTitle("Choose your target device from the list of devices below.");
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));
  layout = NULL;
}

void DeviceSelectPage::initializePage() {
  if (layout != NULL) {
    delete layout;
  }
  layout = new QVBoxLayout;
  drivesLabel.setText("Select Drive:");
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
  GondarWizard* wiz = dynamic_cast<GondarWizard*>(wizard());
  if (wiz->downloadProgressPage.isComplete()) {
    return GondarWizard::Page_writeOperation;
  } else {
    return GondarWizard::Page_downloadProgress;
  }
}

WriteOperationPage::WriteOperationPage(QWidget* parent) : QWizardPage(parent) {
  setTitle("Creating your CloudReady USB installer");
  setSubTitle("This process may take up to 20 minutes.");
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));
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
  image_path.append("chromiumos_image.bin");
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
  setTitle("CloudReady USB created!");
  setSubTitle("You may now either exit or create another USB.");
  qDebug() << "install call returned";
  writeFinished = true;
  progress.setRange(0, 100);
  progress.setValue(100);
  GondarWizard* wiz = dynamic_cast<GondarWizard*>(wizard());
  wiz->showFinishButtons();
  emit completeChanged();
}
