/*
 *   Copyright 2020 Camilo Higuita <milo.h@aol.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.10
import QtQuick.Controls 2.10
import org.kde.mauikit 1.0 as Maui
import org.kde.kirigami 2.7 as Kirigami

Loader
{
    id: control

    /**
      * content : Component
      */
    default property alias content : control.sourceComponent
    active: SwipeView.view.interactive ? SwipeView.isCurrentItem || SwipeView.isPreviousItem || SwipeView.isNextItem || item : SwipeView.isCurrentItem || item
}
