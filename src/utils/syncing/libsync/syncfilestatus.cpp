/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "syncfilestatus.h"

namespace OCC {
SyncFileStatus::SyncFileStatus()
    : _tag(StatusNone)
    , _shared(false)
{
}

SyncFileStatus::SyncFileStatus(SyncFileStatusTag tag)
    : _tag(tag)
    , _shared(false)
{
}

void SyncFileStatus::set(SyncFileStatusTag tag)
{
    _tag = tag;
}

SyncFileStatus::SyncFileStatusTag SyncFileStatus::tag() const
{
    return _tag;
}

void SyncFileStatus::setShared(bool isShared)
{
    _shared = isShared;
}

bool SyncFileStatus::shared() const
{
    return _shared;
}

QString SyncFileStatus::toSocketAPIString() const
{
    QString statusString;
    bool canBeShared = true;

    switch (_tag) {
    case StatusNone:
        statusString = QLatin1String("NOP");
        canBeShared = false;
        break;
    case StatusSync:
        statusString = QLatin1String("SYNC");
        break;
    case StatusWarning:
        // The protocol says IGNORE, but all implementations show a yellow warning sign.
        statusString = QLatin1String("IGNORE");
        break;
    case StatusUpToDate:
        statusString = QLatin1String("OK");
        break;
    case StatusError:
        statusString = QLatin1String("ERROR");
        break;
    }
    if (canBeShared && _shared) {
        statusString += QLatin1String("+SWM");
    }

    return statusString;
}
}
