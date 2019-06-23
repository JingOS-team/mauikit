/*
    This file is part of KDE.

    Copyright 2010 Sebastian Kügler <sebas@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifndef ATTICA_REMOTEACCOUNT_H
#define ATTICA_REMOTEACCOUNT_H

#include <QDate>
#include <QList>
#include <QMap>
#include <QSharedDataPointer>
#include <QStringList>
#include <QUrl>

#ifndef STATIC_MAUIKIT
#include "attica_export.h"
#endif

namespace Attica
{

#ifndef STATIC_MAUIKIT
class ATTICA_EXPORT RemoteAccount
#else
class RemoteAccount
#endif
{
public:
    typedef QList<RemoteAccount> List;
    class Parser;

    RemoteAccount();
    RemoteAccount(const RemoteAccount &other);
    RemoteAccount &operator=(const RemoteAccount &other);
    ~RemoteAccount();

    void setId(const QString &);
    QString id() const;

    void setType(const QString &);
    QString type() const;

    void setRemoteServiceId(const QString &);
    QString remoteServiceId() const;

    void setData(const QString &);
    QString data() const;

    void setLogin(const QString &);
    QString login() const;

    void setPassword(const QString &);
    QString password() const;

    bool isValid() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

}

#endif
