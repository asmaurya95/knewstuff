/*
    knewstuff3/engine.cpp
    Copyright (c) 2007 Josef Spillner <spillner@kde.org>
    Copyright (C) 2007-2010 Frederik Gladhorn <gladhorn@kde.org>
    Copyright (c) 2009 Jeremy Whiting <jpwhiting@kde.org>
    Copyright (c) 2010 Matthias Fuchs <mat69@gmx.net>

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

#include "engine_p.h"

#include "entry.h"
#include "core/installation_p.h"
#include "core/xmlloader_p.h"
#include "ui/imageloader_p.h"

#include <kconfig.h>
#include <kconfiggroup.h>
#include <knewstuff_debug.h>
#include <klocalizedstring.h>
#include <kio/job.h>
#include <QDesktopServices>

#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtXml/qdom.h>
#include <QUrlQuery>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shlobj.h>
#endif

// libattica
#include <attica/providermanager.h>
#include <qstandardpaths.h>

// own
#include "attica/atticaprovider_p.h"
#include "core/cache_p.h"
#include "staticxml/staticxmlprovider_p.h"

using namespace KNS3;

Engine::Engine(QObject *parent)
    : QObject(parent)
    , m_installation(new Installation)
    , m_cache(0)
    , m_searchTimer(new QTimer)
    , m_atticaProviderManager(0)
    , m_currentPage(-1)
    , m_pageSize(20)
    , m_numDataJobs(0)
    , m_numPictureJobs(0)
    , m_numInstallJobs(0)
    , m_initialized(false)
{
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(1000);
    connect(m_searchTimer, &QTimer::timeout, this, &Engine::slotSearchTimerExpired);
    connect(m_installation, &Installation::signalInstallationFinished, this, &Engine::slotInstallationFinished);
    connect(m_installation, &Installation::signalInstallationFailed, this, &Engine::slotInstallationFailed);

}

Engine::~Engine()
{
    if (m_cache) {
        m_cache->writeRegistry();
    }
    delete m_atticaProviderManager;
    delete m_searchTimer;
    delete m_installation;
}

bool Engine::init(const QString &configfile)
{
    qCDebug(KNEWSTUFF) << "Initializing KNS3::Engine from '" << configfile << "'";

    emit signalBusy(i18n("Initializing"));

    KConfig conf(configfile);
    if (conf.accessMode() == KConfig::NoAccess) {
        emit signalError(i18n("Configuration file not found: \"%1\"", configfile));
        qCritical() << "No knsrc file named '" << configfile << "' was found." << endl;
        return false;
    }

    KConfigGroup group;
    if (conf.hasGroup("KNewStuff3")) {
        qCDebug(KNEWSTUFF) << "Loading KNewStuff3 config: " << configfile;
        group = conf.group("KNewStuff3");
    } else if (conf.hasGroup("KNewStuff2")) {
        qCDebug(KNEWSTUFF) << "Loading KNewStuff2 config: " << configfile;
        group = conf.group("KNewStuff2");
    } else {
        emit signalError(i18n("Configuration file is invalid: \"%1\"", configfile));
        qCritical() << "A knsrc file was found but it doesn't contain a KNewStuff3 section." << endl;
        return false;
    }

    m_categories = group.readEntry("Categories", QStringList());

    qCDebug(KNEWSTUFF) << "Categories: " << m_categories;
    m_providerFileUrl = group.readEntry("ProvidersUrl", QString());
    m_applicationName = QFileInfo(QStandardPaths::locate(QStandardPaths::GenericConfigLocation, configfile)).baseName() + ':';

    // let installation read install specific config
    if (!m_installation->readConfig(group)) {
        return false;
    }

    connect(m_installation, &Installation::signalEntryChanged, this, &Engine::slotEntryChanged);

    m_cache = Cache::getCache(m_applicationName.split(':')[0]);
    connect(this, &Engine::signalEntryChanged, m_cache.data(), &Cache::registerChangedEntry);
    m_cache->readRegistry();

    m_initialized = true;

    // load the providers
    loadProviders();

    return true;
}

QStringList Engine::categories() const
{
    return m_categories;
}

QStringList Engine::categoriesFilter() const
{
    return m_currentRequest.categories;
}

void Engine::loadProviders()
{
    if (m_providerFileUrl.isEmpty()) {
        // it would be nicer to move the attica stuff into its own class
        qCDebug(KNEWSTUFF) << "Using OCS default providers";
        delete m_atticaProviderManager;
        m_atticaProviderManager = new Attica::ProviderManager;
        connect(m_atticaProviderManager, &Attica::ProviderManager::providerAdded, this, &Engine::atticaProviderLoaded);
        m_atticaProviderManager->loadDefaultProviders();
    } else {
        qCDebug(KNEWSTUFF) << "loading providers from " << m_providerFileUrl;
        emit signalBusy(i18n("Loading provider information"));

        XmlLoader *loader = new XmlLoader(this);
        connect(loader, &XmlLoader::signalLoaded, this, &Engine::slotProviderFileLoaded);
        connect(loader, &XmlLoader::signalFailed, this, &Engine::slotProvidersFailed);

        loader->load(QUrl(m_providerFileUrl));
    }
}

void Engine::slotProviderFileLoaded(const QDomDocument &doc)
{
    qCDebug(KNEWSTUFF) << "slotProvidersLoaded";

    bool isAtticaProviderFile = false;

    // get each provider element, and create a provider object from it
    QDomElement providers = doc.documentElement();

    if (providers.tagName() == QLatin1String("providers")) {
        isAtticaProviderFile = true;
    } else if (providers.tagName() != QLatin1String("ghnsproviders") && providers.tagName() != QLatin1String("knewstuffproviders")) {
        qWarning() << "No document in providers.xml.";
        emit signalError(i18n("Could not load get hot new stuff providers from file: %1", m_providerFileUrl));
        return;
    }

    QDomElement n = providers.firstChildElement(QStringLiteral("provider"));
    while (!n.isNull()) {
        qCDebug(KNEWSTUFF) << "Provider attributes: " << n.attribute(QStringLiteral("type"));

        QSharedPointer<KNS3::Provider> provider;
        if (isAtticaProviderFile || n.attribute(QStringLiteral("type")).toLower() == QLatin1String("rest")) {
            provider = QSharedPointer<KNS3::Provider> (new AtticaProvider(m_categories));
        } else {
            provider = QSharedPointer<KNS3::Provider> (new StaticXmlProvider);
        }

        if (provider->setProviderXML(n)) {
            addProvider(provider);
        } else {
            emit signalError(i18n("Error initializing provider."));
        }
        n = n.nextSiblingElement();
    }
    emit signalBusy(i18n("Loading data"));
}

void Engine::atticaProviderLoaded(const Attica::Provider &atticaProvider)
{
    qCDebug(KNEWSTUFF) << "atticaProviderLoaded called";
    if (!atticaProvider.hasContentService()) {
        qCDebug(KNEWSTUFF) << "Found provider: " << atticaProvider.baseUrl() << " but it does not support content";
        return;
    }
    QSharedPointer<KNS3::Provider> provider =
        QSharedPointer<KNS3::Provider> (new AtticaProvider(atticaProvider, m_categories));
    addProvider(provider);
}

void Engine::addProvider(QSharedPointer<KNS3::Provider> provider)
{
    qCDebug(KNEWSTUFF) << "Engine addProvider called with provider with id " << provider->id();
    m_providers.insert(provider->id(), provider);
    connect(provider.data(), &Provider::providerInitialized, this, &Engine::providerInitialized);
    connect(provider.data(), SIGNAL(loadingFinished(KNS3::Provider::SearchRequest,KNS3::EntryInternal::List)),
            SLOT(slotEntriesLoaded(KNS3::Provider::SearchRequest,KNS3::EntryInternal::List)));
    connect(provider.data(), &Provider::entryDetailsLoaded, this, &Engine::slotEntryDetailsLoaded);
    connect(provider.data(), &Provider::payloadLinkLoaded, this, &Engine::downloadLinkLoaded);
    connect(provider.data(), &Provider::signalError, this, &Engine::signalError);
    connect(provider.data(), &Provider::signalInformation, this, &Engine::signalIdle);
}

void Engine::providerJobStarted(KJob *job)
{
    emit jobStarted(job, i18n("Loading data from provider"));
}

void Engine::slotProvidersFailed()
{
    emit signalError(i18n("Loading of providers from file: %1 failed", m_providerFileUrl));
}

void Engine::providerInitialized(Provider *p)
{
    qCDebug(KNEWSTUFF) << "providerInitialized" << p->name();
    p->setCachedEntries(m_cache->registryForProvider(p->id()));
    updateStatus();

    foreach (const QSharedPointer<KNS3::Provider> &p, m_providers) {
        if (!p->isInitialized()) {
            return;
        }
    }
    emit signalProvidersLoaded();
}

void Engine::slotEntriesLoaded(const KNS3::Provider::SearchRequest &request, KNS3::EntryInternal::List entries)
{
    m_currentPage = qMax<int>(request.page, m_currentPage);
    qCDebug(KNEWSTUFF) << "loaded page " << request.page << "current page" << m_currentPage;

    if (request.sortMode == Provider::Updates) {
        emit signalUpdateableEntriesLoaded(entries);
    } else {
        m_cache->insertRequest(request, entries);
        emit signalEntriesLoaded(entries);
    }

    --m_numDataJobs;
    updateStatus();
}

void Engine::reloadEntries()
{
    emit signalResetView();
    m_currentPage = -1;
    m_currentRequest.page = 0;
    m_numDataJobs = 0;

    foreach (const QSharedPointer<KNS3::Provider> &p, m_providers) {
        if (p->isInitialized()) {
            if (m_currentRequest.sortMode == Provider::Installed) {
                // when asking for installed entries, never use the cache
                p->loadEntries(m_currentRequest);
            } else {
                // take entries from cache until there are no more
                EntryInternal::List cache = m_cache->requestFromCache(m_currentRequest);
                while (!cache.isEmpty()) {
                    qCDebug(KNEWSTUFF) << "From cache";
                    emit signalEntriesLoaded(cache);

                    m_currentPage = m_currentRequest.page;
                    ++m_currentRequest.page;
                    cache = m_cache->requestFromCache(m_currentRequest);
                }

                // Since the cache has no more pages, reset the request's page
                if (m_currentPage >= 0) {
                    m_currentRequest.page = m_currentPage;
                }

                // if the cache was empty, request data from provider
                if (m_currentPage == -1) {
                    qCDebug(KNEWSTUFF) << "From provider";
                    p->loadEntries(m_currentRequest);

                    ++m_numDataJobs;
                    updateStatus();
                }
            }
        }
    }
}

void Engine::setCategoriesFilter(const QStringList &categories)
{
    m_currentRequest.categories = categories;
    reloadEntries();
}

void Engine::setSortMode(Provider::SortMode mode)
{
    if (m_currentRequest.sortMode != mode) {
        m_currentRequest.page = -1;
    }
    m_currentRequest.sortMode = mode;
    reloadEntries();
}

void Engine::setSearchTerm(const QString &searchString)
{
    m_searchTimer->stop();
    m_currentRequest.searchTerm = searchString;
    EntryInternal::List cache = m_cache->requestFromCache(m_currentRequest);
    if (!cache.isEmpty()) {
        reloadEntries();
    } else {
        m_searchTimer->start();
    }
}

void Engine::slotSearchTimerExpired()
{
    reloadEntries();
}

void Engine::requestMoreData()
{
    qCDebug(KNEWSTUFF) << "Get more data! current page: " << m_currentPage  << " requested: " << m_currentRequest.page;

    if (m_currentPage < m_currentRequest.page) {
        return;
    }

    m_currentRequest.page++;
    doRequest();
}

void Engine::requestData(int page, int pageSize)
{
    m_currentRequest.page = page;
    m_currentRequest.pageSize = pageSize;
    doRequest();
}

void Engine::doRequest()
{
    foreach (const QSharedPointer<KNS3::Provider> &p, m_providers) {
        if (p->isInitialized()) {
            p->loadEntries(m_currentRequest);
            ++m_numDataJobs;
            updateStatus();
        }
    }
}

void Engine::install(KNS3::EntryInternal entry, int linkId)
{
    if (entry.status() == Entry::Updateable) {
        entry.setStatus(Entry::Updating);
    } else  {
        entry.setStatus(Entry::Installing);
    }
    emit signalEntryChanged(entry);

    qCDebug(KNEWSTUFF) << "Install " << entry.name()
       << " from: " << entry.providerId();
    QSharedPointer<Provider> p = m_providers.value(entry.providerId());
    if (p) {
        p->loadPayloadLink(entry, linkId);

        ++m_numInstallJobs;
        updateStatus();
    }
}

void Engine::slotInstallationFinished()
{
    --m_numInstallJobs;
    updateStatus();
}

void Engine::slotInstallationFailed(const QString &message)
{
    --m_numInstallJobs;
    emit signalError(message);
}

void Engine::slotEntryDetailsLoaded(const KNS3::EntryInternal &entry)
{
    emit signalEntryDetailsLoaded(entry);
}

void Engine::downloadLinkLoaded(const KNS3::EntryInternal &entry)
{
    m_installation->install(entry);
}

void Engine::uninstall(KNS3::EntryInternal entry)
{
    KNS3::EntryInternal::List list = m_cache->registryForProvider(entry.providerId());
    //we have to use the cached entry here, not the entry from the provider
    //since that does not contain the list of installed files
    KNS3::EntryInternal actualEntryForUninstall;
    foreach (const KNS3::EntryInternal &eInt, list) {
        if (eInt.uniqueId() == entry.uniqueId()) {
            actualEntryForUninstall = eInt;
            break;
        }
    }
    if (!actualEntryForUninstall.isValid()) {
        qCDebug(KNEWSTUFF) << "could not find a cached entry with following id:" << entry.uniqueId() <<
                 " ->  using the non-cached version";
        return;
    }

    entry.setStatus(Entry::Installing);
    actualEntryForUninstall.setStatus(Entry::Installing);
    emit signalEntryChanged(entry);

    qCDebug(KNEWSTUFF) << "about to uninstall entry " << entry.uniqueId();
    // FIXME: change the status?
    m_installation->uninstall(actualEntryForUninstall);

    entry.setStatus(Entry::Deleted); //status for actual entry gets set in m_installation->uninstall()
    emit signalEntryChanged(entry);

}

void Engine::loadDetails(const KNS3::EntryInternal &entry)
{
    QSharedPointer<Provider> p = m_providers.value(entry.providerId());
    p->loadEntryDetails(entry);
}

void Engine::loadPreview(const KNS3::EntryInternal &entry, EntryInternal::PreviewType type)
{
    qCDebug(KNEWSTUFF) << "START  preview: " << entry.name() << type;
    ImageLoader *l = new ImageLoader(entry, type, this);
    connect(l, &ImageLoader::signalPreviewLoaded, this, &Engine::slotPreviewLoaded);
    l->start();
    ++m_numPictureJobs;
    updateStatus();
}

void Engine::slotPreviewLoaded(const KNS3::EntryInternal &entry, EntryInternal::PreviewType type)
{
    qCDebug(KNEWSTUFF) << "FINISH preview: " << entry.name() << type;
    emit signalEntryPreviewLoaded(entry, type);
    --m_numPictureJobs;
    updateStatus();
}

void Engine::contactAuthor(const EntryInternal &entry)
{
    if (!entry.author().email().isEmpty()) {
        // invoke mail with the address of the author
        QUrl mailUrl;
        mailUrl.setScheme(QStringLiteral("mailto"));
        mailUrl.setPath(entry.author().email());
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("subject"), i18n("Re: %1", entry.name()));
        mailUrl.setQuery(query);
        QDesktopServices::openUrl(mailUrl);
    } else if (!entry.author().homepage().isEmpty()) {
        QDesktopServices::openUrl(QUrl(entry.author().homepage()));
    }
}

void Engine::slotEntryChanged(const KNS3::EntryInternal &entry)
{
    emit signalEntryChanged(entry);
}

bool Engine::userCanVote(const EntryInternal &entry)
{
    QSharedPointer<Provider> p = m_providers.value(entry.providerId());
    return p->userCanVote();
}

void Engine::vote(const EntryInternal &entry, uint rating)
{
    QSharedPointer<Provider> p = m_providers.value(entry.providerId());
    p->vote(entry, rating);
}

bool Engine::userCanBecomeFan(const EntryInternal &entry)
{
    QSharedPointer<Provider> p = m_providers.value(entry.providerId());
    return p->userCanBecomeFan();
}

void Engine::becomeFan(const EntryInternal &entry)
{
    QSharedPointer<Provider> p = m_providers.value(entry.providerId());
    p->becomeFan(entry);
}

void Engine::updateStatus()
{
    if (m_numDataJobs > 0) {
        emit signalBusy(i18n("Loading data"));
    } else if (m_numPictureJobs > 0) {
        emit signalBusy(i18np("Loading one preview", "Loading %1 previews", m_numPictureJobs));
    } else if (m_numInstallJobs > 0) {
        emit signalBusy(i18n("Installing"));
    } else {
        emit signalIdle(QString());
    }
}

void Engine::checkForUpdates()
{
    foreach (QSharedPointer<Provider> p, m_providers) {
        Provider::SearchRequest request(KNS3::Provider::Updates);
        p->loadEntries(request);
    }
}

void KNS3::Engine::checkForInstalled()
{
    foreach (QSharedPointer<Provider> p, m_providers) {
        Provider::SearchRequest request(KNS3::Provider::Installed);
        request.page = 0;
        p->loadEntries(request);
    }
}
