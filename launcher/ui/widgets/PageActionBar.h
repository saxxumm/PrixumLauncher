// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QList>
#include <QWidget>

class QAction;
class QMainWindow;
class QMenu;
class WideBar;

/**
 * A row of buttons above a page's content that takes over from its side toolbar:
 * the chosen actions get buttons, everything else on the toolbar moves into a "more" menu.
 * The toolbar stays around hidden, it still builds the context menus.
 */
class PageActionBar : public QWidget {
    Q_OBJECT

   public:
    struct Buttons {
        /// accent colored, first in the row, may be null
        QAction* primary = nullptr;
        /// next to the primary one
        QList<QAction*> left;
        /// at the right edge, before the "more" button
        QList<QAction*> right;
    };

    /// puts the bar above the central widget of the page and hides the toolbar
    static PageActionBar* install(QMainWindow* page, WideBar* toolbar, const Buttons& buttons);

    /// fills the menu with the toolbar's actions in their order, without the excluded ones
    static void fillMenu(QMenu* menu, WideBar* toolbar, const QList<QAction*>& exclude);

   private:
    PageActionBar(WideBar* toolbar, const Buttons& buttons, QWidget* parent);
};
