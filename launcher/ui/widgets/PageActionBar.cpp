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

#include "PageActionBar.h"

#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

#include "ui/themes/NovaIcons.h"
#include "ui/widgets/WideBar.h"

namespace {

QToolButton* makeButton(QAction* action, const char* role, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setProperty("novaRole", role);
    if (action->menu()) {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }
    return button;
}

}  // namespace

PageActionBar::PageActionBar(WideBar* toolbar, const Buttons& buttons, QWidget* parent) : QWidget(parent)
{
    setObjectName("pageActionBar");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QList<QAction*> shown;
    if (buttons.primary) {
        layout->addWidget(makeButton(buttons.primary, "primary", this));
        shown << buttons.primary;
    }
    for (auto* action : buttons.left) {
        layout->addWidget(makeButton(action, "secondary", this));
        shown << action;
    }
    layout->addStretch(1);
    for (auto* action : buttons.right) {
        layout->addWidget(makeButton(action, "secondary", this));
        shown << action;
    }

    auto* more = new QToolButton(this);
    more->setIcon(NovaIcons::icon("more"));
    more->setToolTip(tr("More actions"));
    more->setProperty("novaRole", "secondary");
    more->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(more);
    connect(menu, &QMenu::aboutToShow, this, [menu, toolbar, shown] { fillMenu(menu, toolbar, shown); });
    more->setMenu(menu);
    layout->addWidget(more);

    // nothing left for the menu, the button would open an empty one
    fillMenu(menu, toolbar, shown);
    more->setVisible(!menu->isEmpty());
}

PageActionBar* PageActionBar::install(QMainWindow* page, WideBar* toolbar, const Buttons& buttons)
{
    toolbar->hide();
    auto* content = page->takeCentralWidget();
    auto* central = new QWidget(page);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto* bar = new PageActionBar(toolbar, buttons, central);
    layout->addWidget(bar);
    layout->addWidget(content, 1);
    page->setCentralWidget(central);
    return bar;
}

void PageActionBar::fillMenu(QMenu* menu, WideBar* toolbar, const QList<QAction*>& exclude)
{
    menu->clear();
    std::unique_ptr<QMenu> all(toolbar->createContextMenu());
    bool pendingSeparator = false;
    for (auto* action : all->actions()) {
        if (action->isSeparator()) {
            pendingSeparator = true;
        } else if (!exclude.contains(action)) {
            // only between actions, a menu with nothing but separators counts as empty
            if (pendingSeparator && !menu->isEmpty()) {
                menu->addSeparator();
            }
            pendingSeparator = false;
            menu->addAction(action);
        }
    }
}
