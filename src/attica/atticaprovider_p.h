/*
    Copyright (c) 2009-2010 Frederik Gladhorn <gladhorn@kde.org>

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
#ifndef KNEWSTUFF3_ATTICAPROVIDER_P_H
#define KNEWSTUFF3_ATTICAPROVIDER_P_H

#include <QtCore/QSet>
#include <QtCore/QPointer>

#include <attica/providermanager.h>
#include <attica/provider.h>
#include <attica/content.h>

#include "core/provider_p.h"

namespace Attica
{
class BaseJob;
}

namespace KNS3
{
/**
 * @short KNewStuff Attica Provider class.
 *
 * This class provides accessors for the provider object.
 * It should not be used directly by the application.
 * This class is the base class and will be instantiated for
 * websites that implement the Open Collaboration Services.
 *
 * @author Frederik Gladhorn <gladhorn@kde.org>
 *
 * @internal
 */
class AtticaProvider: public Provider
{
    Q_OBJECT
public:
    explicit AtticaProvider(const QStringList &categories);
    AtticaProvider(const Attica::Provider &provider, const QStringList &categories);

    QString id() const Q_DECL_OVERRIDE;

    /**
     * set the provider data xml, to initialize the provider
     */
    bool setProviderXML(const QDomElement &xmldata) Q_DECL_OVERRIDE;

    bool isInitialized() const Q_DECL_OVERRIDE;
    void setCachedEntries(const KNS3::EntryInternal::List &cachedEntries) Q_DECL_OVERRIDE;

    void loadEntries(const KNS3::Provider::SearchRequest &request) Q_DECL_OVERRIDE;
    void loadEntryDetails(const KNS3::EntryInternal &entry) Q_DECL_OVERRIDE;
    void loadPayloadLink(const EntryInternal &entry, int linkId) Q_DECL_OVERRIDE;

    bool userCanVote() Q_DECL_OVERRIDE
    {
        return true;
    }
    void vote(const EntryInternal &entry, uint rating) Q_DECL_OVERRIDE;

    bool userCanBecomeFan() Q_DECL_OVERRIDE
    {
        return true;
    }
    void becomeFan(const EntryInternal &entry) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void providerLoaded(const Attica::Provider &provider);
    void listOfCategoriesLoaded(Attica::BaseJob *);
    void categoryContentsLoaded(Attica::BaseJob *job);
    void downloadItemLoaded(Attica::BaseJob *job);
    void accountBalanceLoaded(Attica::BaseJob *job);
    void authenticationCredentialsMissing(const Provider &);
    void votingFinished(Attica::BaseJob *);
    void becomeFanFinished(Attica::BaseJob *job);
    void detailsLoaded(Attica::BaseJob *job);

private:
    void checkForUpdates();
    EntryInternal::List installedEntries() const;
    bool jobSuccess(Attica::BaseJob *job) const;

    Attica::Provider::SortMode atticaSortMode(const SortMode &sortMode);

    EntryInternal entryFromAtticaContent(const Attica::Content &);

    // the attica categories we are interested in (e.g. Wallpaper, Application, Vocabulary File...)
    QHash<QString, Attica::Category> mCategoryMap;

    Attica::ProviderManager m_providerManager;
    Attica::Provider m_provider;

    KNS3::EntryInternal::List mCachedEntries;
    QHash<QString, Attica::Content> mCachedContent;

    // Associate job and entry, this is needed when fetching
    // download links or the account balance in order to continue
    // when the result is there.
    QHash<Attica::BaseJob *, QPair<EntryInternal, int> > mDownloadLinkJobs;

    // keep track of the current request
    QPointer<Attica::BaseJob> mEntryJob;
    Provider::SearchRequest mCurrentRequest;

    QSet<Attica::BaseJob *> m_updateJobs;

    bool mInitialized;

    Q_DISABLE_COPY(AtticaProvider)
};

}

#endif
