/*
    Copyright 2015 by Gregor Mi <codestruct@posteo.org>

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

#include "kmoretoolspresets.h"

#include <QDebug>
#include <QHash>

#include <KNS3/KMoreTools>

class KmtServiceInfo
{
public:
    KmtServiceInfo(QString desktopEntryName, QString homepageUrl)
        : desktopEntryName(desktopEntryName), homepageUrl(homepageUrl)
    {
    }
public:
    QString desktopEntryName;
    QString homepageUrl;
};

//
// todo later: add a property "maturity" with values "stable" > "new" > "incubating" or similar
//
KMoreToolsService* KMoreToolsPresets::registerServiceByDesktopEntryName(KMoreTools* kmt, const QString& desktopEntryName)
{
#define ADD_ENTRY(desktopEntryName, homepageUrl) dict.insert(desktopEntryName, KmtServiceInfo(desktopEntryName, QLatin1String(homepageUrl)));

    static QHash<QString, KmtServiceInfo> dict;

    //
    // definitions begin:
    //
    ADD_ENTRY("git-cola-folder-handler", "https://git-cola.github.io");
    ADD_ENTRY("git-cola-view-history.kmt-edition", "https://git-cola.github.io");
    ADD_ENTRY("gitk.kmt-edition", "http://git-scm.com/docs/gitk");
    ADD_ENTRY("qgit.kmt-edition", "http://libre.tibirna.org/projects/qgit");
    ADD_ENTRY("gitg", "https://wiki.gnome.org/action/show/Apps/Gitg?action=show&redirect=Gitg");
    ADD_ENTRY("gparted", "http://gparted.org");
    ADD_ENTRY("partitionmanager", "http://www.partitionmanager.org");
    ADD_ENTRY("disk", "https://en.opensuse.org/YaST_Disk_Controller");
    ADD_ENTRY("kdf", "https://www.kde.org/applications/system/kdiskfree");
    ADD_ENTRY("org.kde.filelight", "https://utils.kde.org/projects/filelight");
    ADD_ENTRY("hotshots", "http://sourceforge.net/projects/hotshots/");
    ADD_ENTRY("kaption", "http://kde-apps.org/content/show.php/?content=139302");
    ADD_ENTRY("org.kde.kscreengenie", "http://quickgit.kde.org/?p=kscreengenie.git");
    ADD_ENTRY("org.kde.ksnapshot", "https://www.kde.org/applications/graphics/ksnapshot/");
    ADD_ENTRY("shutter", "http://shutter-project.org");
    //
    // ...definitions end
    //

    auto iter = dict.find(desktopEntryName);
    if (iter != dict.end()) {
        auto kmtServiceInfo = *iter;
        const QString subdir = "presets-kmoretools";
        auto serviceLocatingMode = desktopEntryName.endsWith(".kmt-edition") ?
                                   KMoreTools::ServiceLocatingMode_ByProvidedExecLine : KMoreTools::ServiceLocatingMode_Default;
        auto service = kmt->registerServiceByDesktopEntryName(desktopEntryName, subdir, serviceLocatingMode);
        service->setHomepageUrl(QUrl(kmtServiceInfo.homepageUrl));
        return service;
    } else {
        qDebug() << "KMoreToolsPresets::registerServiceByDesktopEntryName: " << desktopEntryName << "was not found. Return nullptr.";
        return nullptr;
    }
}

QList<KMoreToolsService*> KMoreToolsPresets::registerServicesByGroupingName(KMoreTools* kmt, const QStringList& groupingNames)
{
    static QHash<QString, QList<QString>> dict;

    //
    // definitions begin:
    //
    dict.insert("git-clients", { "git-cola-folder-handler", "gitk.kmt-edition", "qgit.kmt-edition", "gitg" });
    dict.insert("git-clients-and-actions", { "git-cola-folder-handler", "git-cola-view-history.kmt-edition", "gitk.kmt-edition", "qgit.kmt-edition", "gitg" });
    dict.insert("disk-usage", { "kdf", "org.kde.filelight" });
    dict.insert("disk-partitions", { "gparted", "partitionmanager", "disk" });
    dict.insert("screenshot-take", { "org.kde.ksnapshot", "org.kde.kscreengenie", "shutter", "kaption", "hotshots" });
    //
    // ...definitions end
    //

    QList<KMoreToolsService*> resultList;

    Q_FOREACH (QString groupingName, groupingNames) {
        auto iter = dict.find(groupingName);
        if (iter != dict.end()) {
            Q_FOREACH(QString desktopEntryName, *iter) {
                resultList << registerServiceByDesktopEntryName(kmt, desktopEntryName);
            }
        }
        else {
            qDebug() << "KMoreToolsPresets::registerServicesByGroupingName: groupingName not found: " << groupingName;
        }
    }

    if (resultList.isEmpty()) {
        qDebug() << "KMoreToolsPresets::registerServicesByGroupingName: " << groupingNames << ". Nothing found in this groupings. HINT: check for invalid grouping names.";
    }

    return resultList;
}

