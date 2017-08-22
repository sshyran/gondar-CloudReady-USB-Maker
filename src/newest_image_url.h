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

#ifndef NEWEST_IMAGE_URL_H
#define NEWEST_IMAGE_URL_H

#include <QNetworkReply>
#include <QUrl>

#include "image_metadata.h"

class NewestImageUrl : public QObject {
  Q_OBJECT

 public:
  void fetch();
  bool isReady() const;
  void set32Url(const QUrl& url_in);
  void set64Url(const QUrl& url_in);
  const gondar::ImageMetadata& getImage32() const;
  const gondar::ImageMetadata& getImage64() const;
 signals:
  void errorOccurred();

 protected:
  void handleReply(QNetworkReply* reply);

 private:
  QNetworkAccessManager networkManager;
  gondar::ImageMetadata image32_;
  gondar::ImageMetadata image64_;
};

#endif
