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

#include <QAbstractButton>

#include "bigtest.h"

#include "src/device_picker.h"
#include "src/gondarwizard.h"
#include "src/log.h"

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

inline void initResource() {
  Q_INIT_RESOURCE(gondarwizard);
}

namespace gondar {

namespace {

class MockDevicePicker : public gondar::DevicePicker {
 private:
  const DevicePicker::Button* selectedButton() const override {
    // honor user selection in case we figure that bit out
    const QAbstractButton* selected = button_group_.checkedButton();
    if (selected) {
      return dynamic_cast<const Button*>(selected);
    } else {
      // let's see if there's a valid choice
      for (auto button : button_group_.buttons()) {
        if (button->isEnabled()) {
          return dynamic_cast<const Button*>(button);
        }
      }
      // the case in which none were enabled
      return NULL;
    }
  }
};

}  // namespace

void proceed(GondarWizard* wizard, int ms = 500) {
  // occasionally with 3 seconds (3k ms) i get stuck on page 3.
  QTest::mouseClick(wizard->button(QWizard::NextButton), Qt::LeftButton,
                    Qt::NoModifier, QPoint(), ms);
  LOG_WARNING << "id after click=" << wizard->currentId();
}

void Test::testDownloadFlow() {
  initResource();
  gondar::InitializeLogging();
  MockDevicePicker* testpicker = new MockDevicePicker();
  GondarWizard wizard(testpicker);
  wizard.show();
  QTest::qWait(1000);
  // 0->3
  proceed(&wizard);
  // 3->4
  proceed(&wizard, 5000);
  // 4->5
  proceed(&wizard);
  // 5->6
  proceed(&wizard);
  // give five minutes for this process to complete, and check for completeness
  // once a second
  for (int i = 0; i < 300; i++) {
    QTest::qWait(1000);
    // if we're not already on page 6, we're going to fail anyway
    // if we're on page 7, we're done
    if (wizard.currentId() != 6 || wizard.currentId() == 7) {
      break;
    }
  }
  // make sure we are on the disk write page, i.e. download has finished
  // successfully
  QVERIFY(wizard.currentId() == 7);
}

}  // namespace gondar

QTEST_MAIN(gondar::Test)
