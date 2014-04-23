/*
    This file is part of KNewStuff.
    Copyright (c) 2002 Cornelius Schumacher <schumacher@kde.org>
    Copyright (c) 2007-2009 Jeremy Whiting <jeremy@scitools.com>
    Copyright (c) 2009 Frederik Gladhorn <gladhorn@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QApplication>
#include <QtDebug>
#include <klocale.h>

#include <iostream>

#include <kns3/downloaddialog.h>

int main(int argc, char **argv)
{
    QCoreApplication::setApplicationName(QLatin1String("khotnewstuff"));
    QCoreApplication::setApplicationVersion(QLatin1String("0.4"));
    QCoreApplication::setOrganizationDomain(QLatin1String("kde.org"));
    QApplication::setApplicationDisplayName(i18n("KHotNewStuff"));

    QApplication i(argc, argv);

    if (i.arguments().count() > 1) {
        QString configfile = QLatin1String(argv[1]);
        KNS3::DownloadDialog dialog(configfile);
        dialog.exec();
        foreach (const KNS3::Entry& e, dialog.changedEntries()) {
            qDebug() << "Changed Entry: " << e.name();
        }
    }
    else
    {
        std::cout << "Enter the filename of a .knsrc file to read configuration from\n";
        return -1;
    }

    return 0;
}

