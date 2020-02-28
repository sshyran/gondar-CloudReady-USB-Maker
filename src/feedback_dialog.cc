// Copyright 2020 Neverware
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

#include "feedback_dialog.h"

#include <QFile>
#include <QJsonDocument>
#include <stdexcept>

#include "log.h"
#include "metric.h"
#include "util.h"

namespace gondar {

FeedbackDialog::FeedbackDialog() {
  feedback_label_.setText("Tell us what you think");
  feedback_label_.setAlignment(Qt::AlignCenter);
  title_label_.setText("Title:");
  details_label_.setText("Details:");

  submit_button_.setText(tr("&Submit"));
  connect(&submit_button_, &QPushButton::clicked, this,
          &FeedbackDialog::submit);
  connect(&network_manager_, &QNetworkAccessManager::finished, this,
          &FeedbackDialog::handleReply);

  layout_.addWidget(&feedback_label_);
  layout_.addWidget(&title_label_);
  layout_.addWidget(&title_);
  layout_.addWidget(&details_label_);
  layout_.addWidget(&details_);
  layout_.addWidget(&submit_button_, 0, Qt::AlignRight);
  setLayout(&layout_);
  setMinimumWidth(500);
  setWindowTitle(tr("CloudReady USB Maker"));
}

static QNetworkRequest createFeedbackRequest() {
  // FIXME(ken): send to the real endpoint once it exists
  auto url = QUrl("http://localhost:7777");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  return request;
}

void FeedbackDialog::handleReply(QNetworkReply* reply) {
  const auto error = reply->error();

  if (error != QNetworkReply::NoError) {
    LOG_ERROR << "Error response from feedback server";
  } else {
    LOG_INFO << "Successfully sent feedback";
  }
}

// we collect:
// the title of the ticket
// the ticket contents
// the user's UUID
// any site information (do we want to only offer this after sign-in phase?)
void FeedbackDialog::submit() {
  LOG_WARNING << "the feedback was accepted";
  auto request = createFeedbackRequest();
  QJsonObject json;
  json["title"] = title_.text();
  json["details"] = details_.toPlainText();
  json["uuid"] = gondar::GetUuid();
  json["site"] = gondar::GetSiteId();
  if (gondar::isChromeover()) {
    json["product"] = "chromeover";
  } else {
    json["product"] = "beerover";
  }
  QJsonDocument doc(json);
  LOG_WARNING << "feedback: " << std::endl
              << doc.toJson(QJsonDocument::Indented);
  network_manager_.post(request, doc.toJson(QJsonDocument::Compact));

  // closes the window
  accept();
}
}  // namespace gondar
