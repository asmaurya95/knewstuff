/*
    Copyright (C) 2016 Dan Leinir Turthra Jensen <admin@leinir.dk>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef HTTPWORKER_H
#define HTTPWORKER_H

#include <QThread>
#include <QUrl>

class QNetworkReply;
namespace KNS3 {

class HTTPWorker : public QObject {
    Q_OBJECT
public:
    enum JobType {
        GetJob
    };
    explicit HTTPWorker(JobType jobType, const QUrl& url, QObject* parent = 0);
    virtual ~HTTPWorker();

    void startRequest();

    void setUrl(const QUrl& url);

    Q_SIGNAL void progress(qlonglong current, qlonglong total);
    Q_SIGNAL void completed();
    Q_SIGNAL void data(const QByteArray& data);

    Q_SLOT void handleReadyRead();
    Q_SLOT void handleFinished(QNetworkReply* reply);
private:
    class Private;
    Private* d;
};

}

#endif//HTTPWORKER_H

