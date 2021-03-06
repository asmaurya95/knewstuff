/*
    knewstuff3/provider.cpp
    Copyright (c) 2002 Cornelius Schumacher <schumacher@kde.org>
    Copyright (c) 2003 - 2007 Josef Spillner <spillner@kde.org>
    Copyright (c) 2009 Jeremy Whiting <jpwhiting@kde.org>
    Copyright (C) 2009-2010 Frederik Gladhorn <gladhorn@kde.org>

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

#include "staticxmlprovider_p.h"

#include "core/xmlloader_p.h"

#include <knewstuff_debug.h>
#include <klocalizedstring.h>

#include <QtCore/QTimer>


namespace KNS3
{

StaticXmlProvider::StaticXmlProvider()
    : mInitialized(false)
{
}

QString StaticXmlProvider::id() const
{
    return mId;
}

bool StaticXmlProvider::setProviderXML(const QDomElement &xmldata)
{
    qCDebug(KNEWSTUFF) << "setting provider xml";

    if (xmldata.tagName() != QLatin1String("provider")) {
        return false;
    }

    mUploadUrl = QUrl(xmldata.attribute(QStringLiteral("uploadurl")));
    mNoUploadUrl = QUrl(xmldata.attribute(QStringLiteral("nouploadurl")));

    QString url = xmldata.attribute(QStringLiteral("downloadurl"));
    if (!url.isEmpty()) {
        mDownloadUrls.insert(QString(), QUrl(url));
    }

    url = xmldata.attribute(QStringLiteral("downloadurl-latest"));
    if (!url.isEmpty()) {
        mDownloadUrls.insert(QStringLiteral("latest"), QUrl(url));
    }

    url = xmldata.attribute(QStringLiteral("downloadurl-score"));
    if (!url.isEmpty()) {
        mDownloadUrls.insert(QStringLiteral("score"), QUrl(url));
    }

    url = xmldata.attribute(QStringLiteral("downloadurl-downloads"));
    if (!url.isEmpty()) {
        mDownloadUrls.insert(QStringLiteral("downloads"), QUrl(url));
    }

    // FIXME: this depends on freedesktop.org icon naming... introduce 'desktopicon'?
    QUrl iconurl(xmldata.attribute(QStringLiteral("icon")));
    if (!iconurl.isValid()) {
        iconurl = QUrl::fromLocalFile(xmldata.attribute(QStringLiteral("icon")));
    }
    mIcon = iconurl;

    QDomNode n;
    for (n = xmldata.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement e = n.toElement();
        if (e.tagName() == QLatin1String("title")) {
            //QString lang = e.attribute("lang");
            mName = e.text().trimmed();
            qCDebug(KNEWSTUFF) << "add name for provider ("<< this << "): " << e.text();
        }
    }

    // Validation
    if ((mNoUploadUrl.isValid()) && (mUploadUrl.isValid())) {
        qWarning() << "StaticXmlProvider: both uploadurl and nouploadurl given";
        return false;
    }

    if ((!mNoUploadUrl.isValid()) && (!mUploadUrl.isValid())) {
        qWarning() << "StaticXmlProvider: neither uploadurl nor nouploadurl given";
        return false;
    }

    mId = mDownloadUrls[QString()].url();
    if (mId.isEmpty()) {
        mId = mDownloadUrls[mDownloadUrls.begin().key()].url();
    }

    QTimer::singleShot(0, this, SLOT(slotEmitProviderInitialized()));

    return true;
}

void StaticXmlProvider::slotEmitProviderInitialized()
{
    mInitialized = true;
    emit providerInitialized(this);
}

bool StaticXmlProvider::isInitialized() const
{
    return mInitialized;
}

void StaticXmlProvider::setCachedEntries(const KNS3::EntryInternal::List &cachedEntries)
{
    qCDebug(KNEWSTUFF) << "Set cached entries " << cachedEntries.size();
    mCachedEntries.append(cachedEntries);
}

void StaticXmlProvider::loadEntries(const KNS3::Provider::SearchRequest &request)
{
    mCurrentRequest = request;

    // static providers only have on page containing everything
    if (request.page > 0) {
        emit loadingFinished(request, EntryInternal::List());
        return;
    }

    if (request.sortMode == Installed) {
        qCDebug(KNEWSTUFF) << "Installed entries: " << mId << installedEntries().size();
        emit loadingFinished(request, installedEntries());
        return;
    }

    QUrl url = downloadUrl(request.sortMode);
    if (!url.isEmpty()) {
        // TODO first get the entries, then filter with searchString, finally emit the finished signal...
        // FIXME: don't creat an endless number of xmlloaders!
        XmlLoader *loader = new XmlLoader(this);
        connect(loader, &XmlLoader::signalLoaded, this, &StaticXmlProvider::slotFeedFileLoaded);
        connect(loader, &XmlLoader::signalFailed, this, &StaticXmlProvider::slotFeedFailed);

        mFeedLoaders.insert(request.sortMode, loader);

        loader->load(url);
    } else {
        emit loadingFailed(request);
    }
}

QUrl StaticXmlProvider::downloadUrl(SortMode mode) const
{
    QUrl url;
    switch (mode) {
    case Installed: // should just query the registry and not end up here
    case Rating:
        url = mDownloadUrls.value(QStringLiteral("score"));
        break;
    case Alphabetical:
        url = mDownloadUrls.value(QString());
        break;
    case Updates:
    case Newest:
        url = mDownloadUrls.value(QStringLiteral("latest"));
        break;
    case Downloads:
        url = mDownloadUrls.value(QStringLiteral("downloads"));
        break;
    }
    if (url.isEmpty()) {
        url = mDownloadUrls.value(QString());
    }
    return url;
}

void StaticXmlProvider::slotFeedFileLoaded(const QDomDocument &doc)
{
    XmlLoader *loader = qobject_cast<KNS3::XmlLoader *>(sender());
    if (!loader) {
        qWarning() << "Loader not found!";
        emit loadingFailed(mCurrentRequest);
        return;
    }

    // load all the entries from the domdocument given
    EntryInternal::List entries;
    QDomElement element;

    element = doc.documentElement();
    QDomElement n;
    for (n = element.firstChildElement(); !n.isNull(); n = n.nextSiblingElement()) {
        EntryInternal entry;
        entry.setEntryXML(n.toElement());
        entry.setStatus(Entry::Downloadable);
        entry.setProviderId(mId);

        int index = mCachedEntries.indexOf(entry);
        if (index >= 0) {

            EntryInternal cacheEntry = mCachedEntries.takeAt(index);
            // check if updateable
            if ((cacheEntry.status() == Entry::Installed) &&
                    ((cacheEntry.version() != entry.version()) || (cacheEntry.releaseDate() != entry.releaseDate()))) {
                entry.setStatus(Entry::Updateable);
                entry.setUpdateVersion(entry.version());
                entry.setVersion(cacheEntry.version());
                entry.setUpdateReleaseDate(entry.releaseDate());
                entry.setReleaseDate(cacheEntry.releaseDate());
            } else {
                entry.setStatus(cacheEntry.status());
            }
            cacheEntry = entry;
        }
        mCachedEntries.append(entry);

        if (searchIncludesEntry(entry)) {
            entries << entry;
        }
    }
    emit loadingFinished(mCurrentRequest, entries);
}

void StaticXmlProvider::slotFeedFailed()
{
    emit loadingFailed(mCurrentRequest);
}

bool StaticXmlProvider::searchIncludesEntry(const KNS3::EntryInternal &entry) const
{
    if (mCurrentRequest.sortMode == Updates) {
        if (entry.status() != Entry::Updateable) {
            return false;
        }
    }

    if (mCurrentRequest.searchTerm.isEmpty()) {
        return true;
    }
    QString search = mCurrentRequest.searchTerm;
    if (entry.name().contains(search, Qt::CaseInsensitive) ||
            entry.summary().contains(search, Qt::CaseInsensitive) ||
            entry.author().name().contains(search, Qt::CaseInsensitive)
       ) {
        return true;
    }
    return false;
}

void StaticXmlProvider::loadPayloadLink(const KNS3::EntryInternal &entry, int)
{
    qCDebug(KNEWSTUFF) << "Payload: " << entry.payload();
    emit payloadLinkLoaded(entry);
}

EntryInternal::List StaticXmlProvider::installedEntries() const
{
    EntryInternal::List entries;
    foreach (const EntryInternal &entry, mCachedEntries) {
        if (entry.status() == Entry::Installed || entry.status() == Entry::Updateable) {
            entries.append(entry);
        }
    }
    return entries;
}

}

