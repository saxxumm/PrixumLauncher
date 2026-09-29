// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Authors: Andrew Okin
 *               Peterix
 *               Orochimarufan <orochimarufan.x3@gmail.com>
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <memory>

#include <BaseInstance.h>
#include <BuildConfig.h>
#include <DesktopServices.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <icons/IconList.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/AccountList.h>
#include <net/ApiRequest.h>
#include <net/NetJob.h>
#include <news/NewsChecker.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include "InstanceWindow.h"

#include "ui/GuiUtil.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/ExportPackDialog.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/ImportResourceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/NewsDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ThemeEditorDialog.h"
#include "ui/dialogs/skins/SkinManageDialog.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/NovaIcons.h"
#include "ui/themes/NovaTheme.h"
#include "ui/themes/ThemeManager.h"

#include "minecraft/PackProfile.h"
#include "minecraft/VersionFile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "minecraft/mod/tasks/LocalResourceParse.h"

#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"
#include "modplatform/modrinth/ModrinthAPI.h"

#include "KonamiCode.h"

#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"

#include "Json.h"

#include "MMCTime.h"

namespace {
QString profileInUseFilter(const QString& profile, bool used)
{
    if (used) {
        return QObject::tr("%1 (in use)").arg(profile);
    } else {
        return profile;
    }
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(APPLICATION->logo());
    setWindowTitle(APPLICATION->applicationDisplayName());
#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
#endif

    // the main window uses the monochrome Nova icons, they follow the colors of the active theme
    setupNovaIcons();

    // set the menu for the folders help, accounts, and export buttons
    {
        ui->actionFoldersButton->setMenu(ui->foldersMenu);

        ui->actionHelpButton->setMenu(new QMenu(this));
        ui->actionHelpButton->menu()->addActions(ui->helpMenu->actions());
        ui->actionHelpButton->menu()->removeAction(ui->actionCheckUpdate);

        auto exportInstanceMenu = new QMenu(this);
        exportInstanceMenu->addAction(ui->actionExportInstanceZip);
        exportInstanceMenu->addAction(ui->actionExportInstanceMrPack);
        exportInstanceMenu->addAction(ui->actionExportInstanceFlamePack);
        ui->actionExportInstance->setMenu(exportInstanceMenu);
    }

    // hide, disable and show stuff
    {
        ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());
        ui->actionMATRIX->setVisible(!BuildConfig.MATRIX_URL.isEmpty());
        ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());
        ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

        ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

#ifndef Q_OS_MAC
        ui->actionAddToPATH->setVisible(false);
#endif

        // disabled until we have an instance selected
        setInstanceActionsEnabled(false);

        ui->actionViewJavaFolder->setEnabled(BuildConfig.JAVA_DOWNLOADER_ENABLED);
    }

    {  // logs viewing
        connect(ui->actionViewLog, &QAction::triggered, this, [] { APPLICATION->showLogWindow(); });
    }

    // sidebar
    {
        ui->novaBrandName->setText(BuildConfig.LAUNCHER_DISPLAYNAME);
        ui->novaBrandVersion->setText(BuildConfig.printableVersionString());
        ui->novaBrandVersion->setToolTip(BuildConfig.printableVersionString());
        // long names and versions must not push the sidebar wider
        ui->novaBrandName->setMinimumWidth(1);
        ui->novaBrandVersion->setMinimumWidth(1);
        ui->brandLayout->removeItem(ui->brandSpacer);
        ui->brandLayout->setSpacing(8);
        ui->brandLayout->setStretchFactor(ui->brandTextLayout, 1);

        bindButton(ui->addInstanceButton, ui->actionAddInstance, NovaIcons::icon("plus", NovaIcons::Tint::AccentText));
        bindButton(ui->foldersButton, ui->actionFoldersButton);
        bindButton(ui->settingsButton, ui->actionSettings);
        bindButton(ui->themeButton, ui->actionChangeTheme);
        bindButton(ui->newsButton, ui->actionMoreNews);
        bindButton(ui->helpButton, ui->actionHelpButton);
        bindButton(ui->updateButton, ui->actionCheckUpdate);
        bindButton(ui->catButton, ui->actionCAT);
        bindButton(ui->accountButton, ui->actionAccountsButton);
        ui->accountButton->setIconSize(QSize(24, 24));

        // there is no library page switching yet, the button brings you back to the full list
        ui->libraryButton->setIcon(NovaIcons::icon("library"));
        connect(ui->libraryButton, &QPushButton::clicked, this, [this] {
            ui->libraryButton->setChecked(true);
            ui->novaSearch->clear();
            view->setFocus();
        });

        // gamescope / steam deck: it defaults to an X11/XWayland session and does not implement decorations
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
            bindButton(ui->closeWindowButton, ui->actionCloseWindow);
        } else {
            ui->closeWindowButton->hide();
        }

        ui->sidebarToggleButton->setIcon(NovaIcons::icon("menu", NovaIcons::Tint::Muted));
        ui->sidebarToggleButton->setToolTip(ui->actionToggleSidebar->toolTip());
        connect(ui->sidebarToggleButton, &QToolButton::clicked, ui->actionToggleSidebar, &QAction::trigger);
    }

    // instance panel
    {
        bindButton(ui->novaPlayButton, ui->actionLaunchInstance, NovaIcons::icon("play", NovaIcons::Tint::AccentText));
        ui->novaPlayButton->setPopupMode(QToolButton::MenuButtonPopup);
        ui->novaPlayButton->setIconSize(QSize(20, 20));
        bindButton(ui->novaKillButton, ui->actionKillInstance, NovaIcons::icon("stop", NovaIcons::Tint::Danger));
        ui->novaKillButton->setProperty("novaHideWhenDisabled", true);
        syncButton(ui->novaKillButton, ui->actionKillInstance);

        bindButton(ui->editButton, ui->actionEditInstance);
        bindButton(ui->groupButton, ui->actionChangeInstGroup);
        bindButton(ui->folderButton, ui->actionViewSelectedInstFolder);
        // a menu on a push button shifts its contents, pop the export formats up by hand instead
        ui->exportButton->setProperty("novaPopupMenu", true);
        bindButton(ui->exportButton, ui->actionExportInstance);
        bindButton(ui->copyButton, ui->actionCopyInstance);
        bindButton(ui->shortcutButton, ui->actionCreateInstanceShortcut);
        bindButton(ui->deleteButton, ui->actionDeleteInstance, NovaIcons::icon("trash", NovaIcons::Tint::Danger));

        connect(ui->novaInstanceIcon, &QToolButton::clicked, this, &MainWindow::on_actionChangeInstIcon_triggered);
        connect(ui->novaInstanceName, &QToolButton::clicked, this, &MainWindow::on_actionRenameInstance_triggered);
        ui->novaInstanceIcon->setCursor(Qt::PointingHandCursor);
        ui->novaInstanceName->setCursor(Qt::PointingHandCursor);
        ui->inspectorStack->setCurrentWidget(ui->inspectorEmptyPage);
        ui->novaInstanceIcon->setIconSize(QSize(64, 64));
        ui->actionsLayout->setVerticalSpacing(2);
        ui->inspectorInstanceLayout->setSpacing(6);
        ui->detailsLayout->setVerticalSpacing(5);
        ui->inspectorLayout->setContentsMargins(14, 14, 14, 12);
        // the panel only scrolls when the window is really small
        ui->inspectorScroll->viewport()->setAutoFillBackground(false);
        ui->inspectorScrollContents->setAutoFillBackground(false);

        bindButton(ui->inspectorToggleButton, ui->actionToggleInspector, NovaIcons::icon("panel", NovaIcons::Tint::Muted));
    }

    updateThemeMenu();
    updateMainToolBar();
    // OSX magic.
    setUnifiedTitleAndToolBarOnMac(true);

    // Global shortcuts
    {
        // you can't set QKeySequence::StandardKey shortcuts in qt designer >:(
        ui->actionAddInstance->setShortcut(QKeySequence::New);
        ui->actionSettings->setShortcut(QKeySequence::Preferences);
        ui->actionUndoTrashInstance->setShortcut(QKeySequence::Undo);
        ui->actionDeleteInstance->setShortcuts({ QKeySequence(tr("Backspace")), QKeySequence::Delete });
        ui->actionCloseWindow->setShortcut(QKeySequence::Close);
        connect(ui->actionCloseWindow, &QAction::triggered, APPLICATION, &Application::closeCurrentWindow);

        // FIXME: This is kinda weird. and bad. We need some kind of managed shutdown.
        auto q = new QShortcut(QKeySequence::Quit, this);
        connect(q, &QShortcut::activated, APPLICATION, &Application::quit);

        auto find = new QShortcut(QKeySequence::Find, this);
        connect(find, &QShortcut::activated, this, [this] {
            ui->novaSearch->setFocus();
            ui->novaSearch->selectAll();
        });
    }

    // Konami Code
    {
        secretEventFilter = new KonamiCode(this);
        connect(secretEventFilter, &KonamiCode::triggered, this, &MainWindow::konamiTriggered);
    }

    // Add the news label to the news bar.
    {
        m_newsChecker.reset(new NewsChecker(APPLICATION->network(), BuildConfig.NEWS_RSS_URL));
        ui->newsLabel->setFocusPolicy(Qt::NoFocus);
        bindButton(ui->moreNewsButton, ui->actionMoreNews);

        connect(ui->newsLabel, &QAbstractButton::clicked, this, &MainWindow::newsButtonClicked);
        connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this, &MainWindow::updateNewsLabel);
        updateNewsLabel();
    }

    // Create the instance list widget
    {
        view = new InstanceView(ui->novaLibrary);

        view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_delegate = new ListViewDelegate(this);
        view->setItemDelegate(m_delegate);
        view->setFrameShape(QFrame::NoFrame);
        // do not show ugly blue border on the mac
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        connect(m_delegate, &ListViewDelegate::textChanged, this, [this](QString before, QString after) {
            if (auto newRoot = askToUpdateInstanceDirName(m_selectedInstance, before, after, this); !newRoot.isEmpty()) {
                auto oldID = m_selectedInstance->id();
                auto newID = QFileInfo(newRoot).fileName();
                QString origGroup(APPLICATION->instances()->getInstanceGroup(oldID));
                bool syncGroup = origGroup != GroupId() && oldID != newID;
                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(oldID, GroupId());

                refreshInstances();
                setSelectedInstanceById(newID);

                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(newID, origGroup);
            }
        });

        view->installEventFilter(this);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, &MainWindow::showInstanceContextMenu);
        connect(view, &InstanceView::droppedURLs, this, &MainWindow::processURLs, Qt::QueuedConnection);

        proxymodel = new InstanceProxyModel(this);
        proxymodel->setSourceModel(APPLICATION->instances());
        proxymodel->sort(0);
        connect(proxymodel, &InstanceProxyModel::dataChanged, this, &MainWindow::instanceDataChanged);
        connect(proxymodel, &InstanceProxyModel::rowsInserted, this, &MainWindow::updateInstanceCount);
        connect(proxymodel, &InstanceProxyModel::rowsRemoved, this, &MainWindow::updateInstanceCount);
        connect(proxymodel, &InstanceProxyModel::modelReset, this, &MainWindow::updateInstanceCount);
        connect(proxymodel, &InstanceProxyModel::layoutChanged, this, &MainWindow::updateInstanceCount);

        view->setModel(proxymodel);
        view->setSourceOfGroupCollapseStatus(
            [](const QString& groupName) -> bool { return APPLICATION->instances()->isGroupCollapsed(groupName); });
        connect(view, &InstanceView::groupStateChanged, APPLICATION->instances(), &InstanceList::on_GroupStateChanged);
        ui->libraryLayout->addWidget(view);
    }

    // search and sorting
    {
        ui->novaSearch->addAction(NovaIcons::icon("search", NovaIcons::Tint::Muted), QLineEdit::LeadingPosition);
        connect(ui->novaSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
            if (m_selectedInstance) {
                m_filterSelection = m_selectedInstance->id();
            }
            m_filtering = true;
            proxymodel->setFilterText(text);
            m_filtering = false;
            if (!m_filterSelection.isEmpty()) {
                setSelectedInstanceById(m_filterSelection);
            }
            ui->libraryButton->setChecked(true);
            updateInstanceCount();
        });

        ui->novaSort->addItem(NovaIcons::icon("sort", NovaIcons::Tint::Muted), QString(), "Name");
        ui->novaSort->addItem(NovaIcons::icon("sort", NovaIcons::Tint::Muted), QString(), "LastLaunch");
        ui->novaSort->addItem(NovaIcons::icon("sort", NovaIcons::Tint::Muted), QString(), "Playtime");
        ui->novaSort->setCurrentIndex(std::max(0, ui->novaSort->findData(APPLICATION->settings()->get("InstSortMode").toString())));
        connect(ui->novaSort, &QComboBox::currentIndexChanged, this, [this](int index) {
            APPLICATION->settings()->set("InstSortMode", ui->novaSort->itemData(index).toString());
            proxymodel->invalidate();
            proxymodel->sort(0);
        });
    }

    // The cat background
    {
        // set the cat action priority here so you can still see the action in qt designer
        ui->actionCAT->setPriority(QAction::LowPriority);
        updateCatState();
        connect(ui->actionCAT, &QAction::toggled, this, &MainWindow::onCatToggled);
        connect(APPLICATION, &Application::currentCatChanged, this, &MainWindow::onCatChanged);
    }

    // Togglable status bar
    {
        bool statusBarVisible = APPLICATION->settings()->get("StatusBarVisible").toBool();
        ui->actionToggleStatusBar->setChecked(statusBarVisible);
        connect(ui->actionToggleStatusBar, &QAction::toggled, this, &MainWindow::setStatusBarVisibility);
        setStatusBarVisibility(statusBarVisible);
    }

    // Togglable parts of the layout
    {
        const auto settings = APPLICATION->settings();
        ui->actionToggleSidebar->setChecked(settings->get("NovaSidebarCompact").toBool());
        ui->actionToggleInspector->setChecked(settings->get("NovaInspectorVisible").toBool());
        ui->actionToggleNewsBar->setChecked(settings->get("NovaNewsVisible").toBool());
        connect(ui->actionToggleSidebar, &QAction::toggled, this, &MainWindow::setSidebarCompact);
        connect(ui->actionToggleInspector, &QAction::toggled, this, &MainWindow::setInspectorVisibility);
        connect(ui->actionToggleNewsBar, &QAction::toggled, this, &MainWindow::setNewsBarVisibility);
        setSidebarCompact(ui->actionToggleSidebar->isChecked());
        setInspectorVisibility(ui->actionToggleInspector->isChecked());
        setNewsBarVisibility(ui->actionToggleNewsBar->isChecked());
    }

    // start instance when double-clicked
    connect(view, &InstanceView::activated, this, &MainWindow::instanceActivated);

    // track the selection -- update the instance panel
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::instanceChanged);

    // track icon changes and update the instance panel!
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, &MainWindow::iconUpdated);

    // model reset -> selection is invalid. All the instance pointers are wrong.
    connect(APPLICATION->instances(), &InstanceList::dataIsInvalid, this, &MainWindow::selectionBad);

    // handle newly added instances
    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, &MainWindow::instanceSelectRequest);

    // When the global settings page closes, we want to know about it and update our state
    connect(APPLICATION, &Application::globalSettingsApplied, this, &MainWindow::globalSettingsClosed);

    // sizes of the sidebar, panels and instance tiles are part of the theme
    connect(APPLICATION, &Application::themeApplied, this, &MainWindow::applyThemeMetrics);

    m_statusLeft = new QLabel(tr("No instance selected"), this);
    m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
    statusBar()->addPermanentWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusCenter, 0);

    // Use undocumented property... https://stackoverflow.com/questions/7121718/create-a-scrollbar-in-a-submenu-qt
    ui->accountsMenu->setStyleSheet("QMenu { menu-scrollable: 1; }");

    repopulateAccountsMenu();

    // Update the menu when the active account changes.
    // Shouldn't have to use lambdas here like this, but if I don't, the compiler throws a fit.
    // Template hell sucks...
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, this, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listActivityChanged, this, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listChanged, this, [this] { defaultAccountChanged(); });

    // Show initial account
    defaultAccountChanged();

    // TODO: refresh accounts here?
    // auto accounts = APPLICATION->accounts();

    // load the news
    {
        m_newsChecker->reloadNews();
        updateNewsLabel();
    }

    if (APPLICATION->updaterEnabled()) {
        bool updatesAllowed = APPLICATION->updatesAreAllowed();
        updatesAllowedChanged(updatesAllowed);

        connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::checkForUpdates);

        // set up the updater object.
        auto updater = APPLICATION->updater();

        if (updater) {
            connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this, &MainWindow::updatesAllowedChanged);
        }
    }

    connect(ui->actionUndoTrashInstance, &QAction::triggered, this, &MainWindow::undoTrashInstance);

    applyThemeMetrics();
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());

    // removing this looks stupid
    view->setFocus();

    retranslateUi();
}

void MainWindow::setupNovaIcons()
{
    using NovaIcons::icon;
    ui->actionAddInstance->setIcon(icon("plus"));
    ui->actionFoldersButton->setIcon(icon("folder"));
    ui->actionSettings->setIcon(icon("settings"));
    ui->actionChangeTheme->setIcon(icon("palette"));
    ui->actionThemeEditor->setIcon(icon("sparkles"));
    ui->actionHelpButton->setIcon(icon("help"));
    ui->actionCheckUpdate->setIcon(icon("update"));
    ui->actionCAT->setIcon(icon("cat"));
    ui->actionMoreNews->setIcon(icon("news"));
    ui->actionAccountsButton->setIcon(icon("user"));
    ui->actionManageAccounts->setIcon(icon("account-add"));
    ui->actionLaunchInstance->setIcon(icon("play"));
    ui->actionKillInstance->setIcon(icon("stop"));
    ui->actionEditInstance->setIcon(icon("edit"));
    ui->actionChangeInstGroup->setIcon(icon("tag"));
    ui->actionViewSelectedInstFolder->setIcon(icon("folder"));
    ui->actionExportInstance->setIcon(icon("export"));
    ui->actionCopyInstance->setIcon(icon("copy"));
    ui->actionDeleteInstance->setIcon(icon("trash"));
    ui->actionCreateInstanceShortcut->setIcon(icon("shortcut"));
    ui->actionRenameInstance->setIcon(icon("rename"));
    ui->actionViewLog->setIcon(icon("logs"));
    ui->actionCloseWindow->setIcon(icon("close"));
    ui->actionToggleSidebar->setIcon(icon("menu"));
    ui->actionToggleInspector->setIcon(icon("panel"));
}

void MainWindow::bindButton(QAbstractButton* button, QAction* action, const QIcon& icon)
{
    button->setProperty("novaIcon", QVariant::fromValue(icon));
    m_boundButtons.append({ button, action });
    syncButton(button, action);
    connect(action, &QAction::changed, button, [this, button, action] { syncButton(button, action); });
    // buttons with a menu show it instead of emitting clicked (except for the split play button)
    connect(button, &QAbstractButton::clicked, action, [button, action] {
        if (button->property("novaPopupMenu").toBool() && action->menu()) {
            action->menu()->popup(button->mapToGlobal(QPoint(0, button->height())));
        } else {
            action->trigger();
        }
    });
}

void MainWindow::syncButton(QAbstractButton* button, QAction* action)
{
    const bool compact = button->property("compact").toBool();
    button->setText(compact ? QString() : action->iconText());
    button->setToolTip(compact ? action->iconText() : action->toolTip());
    button->setEnabled(action->isEnabled());
    if (button->property("novaHideWhenDisabled").toBool()) {
        button->setVisible(action->isVisible() && action->isEnabled());
    } else {
        button->setVisible(action->isVisible());
    }
    button->setCheckable(action->isCheckable());
    if (action->isCheckable()) {
        button->setChecked(action->isChecked());
    }
    const auto icon = button->property("novaIcon").value<QIcon>();
    button->setIcon(icon.isNull() ? action->icon() : icon);
    if (button->property("novaPopupMenu").toBool()) {
        // handled in bindButton
    } else if (auto* push = qobject_cast<QPushButton*>(button)) {
        if (push->menu() != action->menu()) {
            push->setMenu(action->menu());
        }
    } else if (auto* tool = qobject_cast<QToolButton*>(button)) {
        if (tool->menu() != action->menu()) {
            tool->setMenu(action->menu());
        }
    }
}

void MainWindow::applyThemeMetrics()
{
    const auto tokens = Nova::current();
    const qreal dpr = devicePixelRatioF();

    ui->novaSidebar->setFixedWidth(ui->actionToggleSidebar->isChecked() ? 76 : tokens.metric("sidebarWidth"));
    ui->novaInspector->setFixedWidth(tokens.metric("inspectorWidth"));

    m_delegate->setMetrics(tokens.metric("cardWidth"), tokens.metric("iconSize"));
    view->setItemWidth(tokens.metric("cardWidth"));
    view->updateGeometries();
    view->viewport()->update();

    ui->brandIcon->setPixmap(APPLICATION->logo().pixmap(QSize(34, 34), dpr));
    // room next to the logo and the collapse button
    const int brandWidth = tokens.metric("sidebarWidth") - 24 - 4 - 34 - 16 - 30;
    ui->novaBrandName->setText(ui->novaBrandName->fontMetrics().elidedText(BuildConfig.LAUNCHER_DISPLAYNAME, Qt::ElideRight, brandWidth));
    ui->novaBrandVersion->setText(
        ui->novaBrandVersion->fontMetrics().elidedText(BuildConfig.printableVersionString(), Qt::ElideRight, brandWidth));
    ui->emptyInspectorIcon->setPixmap(NovaIcons::icon("cube", NovaIcons::Tint::Muted).pixmap(QSize(44, 44), dpr));
    ui->newsIcon->setPixmap(NovaIcons::icon("news", NovaIcons::Tint::Muted).pixmap(QSize(16, 16), dpr));
    ui->versionIcon->setPixmap(NovaIcons::icon("cube", NovaIcons::Tint::Muted).pixmap(QSize(16, 16), dpr));
    ui->playtimeIcon->setPixmap(NovaIcons::icon("clock", NovaIcons::Tint::Muted).pixmap(QSize(16, 16), dpr));
    ui->lastPlayedIcon->setPixmap(NovaIcons::icon("history", NovaIcons::Tint::Muted).pixmap(QSize(16, 16), dpr));
    ui->groupIcon->setPixmap(NovaIcons::icon("tag", NovaIcons::Tint::Muted).pixmap(QSize(16, 16), dpr));
    updateInspector();
}

void MainWindow::setSidebarCompact(bool compact)
{
    APPLICATION->settings()->set("NovaSidebarCompact", compact);
    for (auto& [button, action] : m_boundButtons) {
        if (ui->novaSidebar->isAncestorOf(button)) {
            button->setProperty("compact", compact);
            syncButton(button, action);
            // re-evaluate the [compact="true"] selectors
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }
    ui->libraryButton->setText(compact ? QString() : tr("Instances"));
    ui->libraryButton->setToolTip(compact ? tr("Instances") : QString());
    ui->libraryButton->setProperty("compact", compact);
    ui->libraryButton->style()->unpolish(ui->libraryButton);
    ui->libraryButton->style()->polish(ui->libraryButton);
    ui->brandIcon->setVisible(!compact);
    ui->novaBrandName->setVisible(!compact);
    ui->novaBrandVersion->setVisible(!compact);
    ui->librarySectionLabel->setVisible(!compact);
    ui->moreSectionLabel->setVisible(!compact);
    ui->novaSidebar->setFixedWidth(compact ? 76 : Nova::current().metric("sidebarWidth"));
}

void MainWindow::setInspectorVisibility(bool visible)
{
    APPLICATION->settings()->set("NovaInspectorVisible", visible);
    ui->novaInspector->setVisible(visible);
}

void MainWindow::setNewsBarVisibility(bool visible)
{
    APPLICATION->settings()->set("NovaNewsVisible", visible);
    ui->novaNewsBar->setVisible(visible);
}

void MainWindow::on_actionThemeEditor_triggered()
{
    ThemeEditorDialog dialog(this);
    dialog.exec();
    updateThemeMenu();
}

void MainWindow::updateInstanceCount()
{
    const int total = APPLICATION->instances()->count();
    const int shown = proxymodel->rowCount();
    if (proxymodel->filterText().isEmpty()) {
        ui->instanceCountLabel->setText(QString::number(total));
        view->setEmptyText(tr("Welcome!"), tr("Click \"Add Instance\" to get started."));
    } else {
        ui->instanceCountLabel->setText(QString("%1 / %2").arg(shown).arg(total));
        view->setEmptyText(tr("Nothing found"), tr("No instance matches \"%1\".").arg(proxymodel->filterText()));
    }
}

void MainWindow::updateInspector()
{
    if (!m_selectedInstance) {
        ui->inspectorStack->setCurrentWidget(ui->inspectorEmptyPage);
        return;
    }
    ui->inspectorStack->setCurrentWidget(ui->inspectorInstancePage);
    auto* instance = m_selectedInstance;

    const int nameWidth = std::max(80, ui->novaInspector->width() - 48);
    ui->novaInstanceName->setText(ui->novaInstanceName->fontMetrics().elidedText(instance->name(), Qt::ElideRight, nameWidth));

    QString state = "idle";
    QString status = tr("Ready to play");
    if (instance->isRunning()) {
        state = "running";
        status = tr("Running");
    } else if (instance->hasVersionBroken()) {
        state = "broken";
        status = tr("Broken");
    } else if (instance->hasCrashed()) {
        state = "broken";
        status = tr("Crashed");
    }
    ui->novaStatusPill->setText(status);
    ui->novaStatusPill->setProperty("state", state);
    ui->novaStatusPill->style()->unpolish(ui->novaStatusPill);
    ui->novaStatusPill->style()->polish(ui->novaStatusPill);

    // version and mod loader
    {
        auto profile = instance->getPackProfile();
        QString version = profile->getComponentVersion("net.minecraft");
        version = version.isEmpty() ? tr("Unknown version") : tr("Minecraft %1").arg(version);
        static const QList<std::pair<QString, QString>> s_loaders{ { "net.neoforged", "NeoForge" },
                                                                   { "net.minecraftforge", "Forge" },
                                                                   { "net.fabricmc.fabric-loader", "Fabric" },
                                                                   { "org.quiltmc.quilt-loader", "Quilt" },
                                                                   { "com.mumfrey.liteloader", "LiteLoader" } };
        for (const auto& [uid, loader] : s_loaders) {
            if (auto loaderVersion = profile->getComponentVersion(uid); !loaderVersion.isEmpty()) {
                version += QString(" · %1 %2").arg(loader, loaderVersion);
                break;
            }
        }
        ui->versionLabel->setText(version);
    }

    const bool showTime = instance->settings()->get("ShowGameTime").toBool();
    ui->playtimeIcon->setVisible(showTime);
    ui->playtimeLabel->setVisible(showTime);
    ui->lastPlayedIcon->setVisible(showTime);
    ui->lastPlayedLabel->setVisible(showTime);
    if (showTime) {
        const bool withoutDays = APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool();
        ui->playtimeLabel->setText(instance->totalTimePlayed() > 0
                                       ? tr("Played for %1").arg(Time::prettifyDuration(instance->totalTimePlayed(), withoutDays))
                                       : tr("Never played"));
        ui->lastPlayedLabel->setText(
            instance->lastLaunch() > 0
                ? tr("Last played %1")
                      .arg(QLocale().toString(QDateTime::fromMSecsSinceEpoch(instance->lastLaunch()).date(), QLocale::ShortFormat))
                : tr("Not launched yet"));
    }

    const QString group = APPLICATION->instances()->getInstanceGroup(instance->id());
    ui->groupLabel->setText(group.isEmpty() ? tr("Ungrouped") : group);
}

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt && !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool())
        ui->menuBar->setVisible(!ui->menuBar->isVisible());
    else
        QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::retranslateUi()
{
    if (m_selectedInstance) {
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
    } else {
        m_statusLeft->setText(tr("No instance selected"));
    }

    ui->retranslateUi(this);

    MinecraftAccountPtr defaultAccount = APPLICATION->accounts()->defaultAccount();
    if (defaultAccount) {
        auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
    }

    ui->actionExportInstanceZip->setText(tr("%1 (zip)").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    ui->novaInstanceIcon->setToolTip(ui->actionChangeInstIcon->toolTip());
    ui->novaInstanceName->setToolTip(ui->actionRenameInstance->toolTip());
    ui->librarySectionLabel->setText(ui->librarySectionLabel->text().toUpper());
    ui->moreSectionLabel->setText(ui->moreSectionLabel->text().toUpper());
    ui->novaSort->setItemText(0, tr("Name"));
    ui->novaSort->setItemText(1, tr("Last launched"));
    ui->novaSort->setItemText(2, tr("Playtime"));
    ui->novaSort->setToolTip(tr("Sort instances by"));
    if (!ui->actionToggleSidebar->isChecked()) {
        ui->libraryButton->setText(tr("Instances"));
    }
    updateInstanceCount();
    updateInspector();

    // replace the %1 with the launcher display name in some actions
    if (ui->actionHelpButton->toolTip().contains("%1"))
        ui->actionHelpButton->setToolTip(ui->actionHelpButton->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));

    for (auto action : ui->helpMenu->actions()) {
        if (action->text().contains("%1"))
            action->setText(action->text().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        if (action->toolTip().contains("%1"))
            action->setToolTip(action->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}

MainWindow::~MainWindow() {}

QMenu* MainWindow::createPopupMenu()
{
    auto* menu = new QMenu(this);
    menu->addAction(ui->actionToggleSidebar);
    menu->addAction(ui->actionToggleInspector);
    menu->addAction(ui->actionToggleNewsBar);
    menu->addAction(ui->actionToggleStatusBar);
    return menu;
}
void MainWindow::setStatusBarVisibility(bool state)
{
    statusBar()->setVisible(state);
    APPLICATION->settings()->set("StatusBarVisible", state);
}

void MainWindow::konamiTriggered()
{
    QString gradient =
        " stop:0 rgba(125, 0, 0, 255), stop:0.166 rgba(125, 125, 0, 255), stop:0.333 rgba(0, 125, 0, 255), stop:0.5 rgba(0, 125, 125, "
        "255), stop:0.666 rgba(0, 0, 125, 255), stop:0.833 rgba(125, 0, 125, 255), stop:1 rgba(125, 0, 0, 255));";
    QString stylesheet = "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0," + gradient;
    const QString sidebarStyle =
        "QFrame#novaSidebar { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1," + gradient + " }";
    if (ui->novaSidebar->styleSheet() == sidebarStyle) {
        ui->novaSidebar->setStyleSheet("");
        ui->novaCentral->setStyleSheet("");
        ui->novaNewsBar->setStyleSheet("");
        ui->statusBar->setStyleSheet("");
        qDebug() << "Super Secret Mode DEACTIVATED!";
    } else {
        ui->novaSidebar->setStyleSheet(sidebarStyle);
        ui->novaCentral->setStyleSheet("QWidget#novaCentral { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1," +
                                       gradient + " }");
        ui->novaNewsBar->setStyleSheet("QFrame#novaNewsBar { " + stylesheet + " }");
        ui->statusBar->setStyleSheet(stylesheet);
        qDebug() << "Super Secret Mode ACTIVATED!";
    }
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
    QList<QAction*> actions;

    QAction* actionSep = new QAction("", this);
    actionSep->setSeparator(true);

    bool onInstance = view->indexAt(pos).isValid();
    if (onInstance) {
        // reuse the file menu actions
        actions = ui->fileMenu->actions();

        // remove the add instance action, launcher settings action and close action
        actions.removeFirst();
        actions.removeLast();
        actions.removeLast();

        actions.prepend(ui->actionChangeInstIcon);
        actions.prepend(ui->actionRenameInstance);

        // add header
        actions.prepend(actionSep);
        QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
        actionVoid->setEnabled(false);
        actions.prepend(actionVoid);
    } else {
        auto group = view->groupNameAt(pos);

        QAction* actionVoid = new QAction(group.isNull() ? BuildConfig.LAUNCHER_DISPLAYNAME : group, this);
        actionVoid->setEnabled(false);

        QAction* actionCreateInstance = new QAction(tr("&Create instance"), this);
        actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
        if (!group.isNull()) {
            QVariantMap instance_action_data;
            instance_action_data["group"] = group;
            actionCreateInstance->setData(instance_action_data);
        }

        connect(actionCreateInstance, &QAction::triggered, this, &MainWindow::on_actionAddInstance_triggered);

        actions.prepend(actionSep);
        actions.prepend(actionVoid);
        actions.append(actionCreateInstance);
        if (!group.isNull()) {
            QAction* actionDeleteGroup = new QAction(tr("&Delete group"), this);
            connect(actionDeleteGroup, &QAction::triggered, this, [this, group] { deleteGroup(group); });
            actions.append(actionDeleteGroup);

            QAction* actionRenameGroup = new QAction(tr("&Rename group"), this);
            connect(actionRenameGroup, &QAction::triggered, this, [this, group] { renameGroup(group); });
            actions.append(actionRenameGroup);
        }
    }
    QMenu myMenu;
    myMenu.addActions(actions);
    /*
    if (onInstance)
        myMenu.setEnabled(m_selectedInstance->canLaunch());
    */
    myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateMainToolBar()
{
    // the sidebar replaces the old main toolbar, the classic menu bar can still be shown on top of it
    ui->menuBar->setVisible(APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
}

void MainWindow::updateLaunchButton()
{
    QMenu* launchMenu = ui->actionLaunchInstance->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    if (m_selectedInstance)
        m_selectedInstance->populateLaunchMenu(launchMenu);
    ui->actionLaunchInstance->setMenu(launchMenu);
}

void MainWindow::updateThemeMenu()
{
    QMenu* themeMenu = ui->actionChangeTheme->menu();

    if (themeMenu) {
        themeMenu->clear();
    } else {
        themeMenu = new QMenu(this);
    }

    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();

    QActionGroup* themesGroup = new QActionGroup(this);

    // Nova themes first, the classic ones below
    for (const bool nova : { true, false }) {
        if (!nova) {
            themeMenu->addSeparator();
        }
        for (auto* theme : themes) {
            if ((dynamic_cast<NovaTheme*>(theme) != nullptr) != nova) {
                continue;
            }
            QAction* themeAction = themeMenu->addAction(theme->name());
            themeAction->setToolTip(theme->tooltip());

            themeAction->setCheckable(true);
            if (APPLICATION->settings()->get("ApplicationTheme").toString() == theme->id()) {
                themeAction->setChecked(true);
            }
            themeAction->setActionGroup(themesGroup);

            // themes can be reloaded while the menu exists, so only keep the id around
            connect(themeAction, &QAction::triggered, APPLICATION, [id = theme->id()]() {
                APPLICATION->themeManager()->setApplicationTheme(id);
                APPLICATION->settings()->set("ApplicationTheme", id);
            });
        }
    }
    themeMenu->addSeparator();
    themeMenu->addAction(ui->actionThemeEditor);
    themeMenu->addAction(ui->actionViewWidgetThemeFolder);

    ui->actionChangeTheme->setMenu(themeMenu);
}

void MainWindow::repopulateAccountsMenu()
{
    ui->accountsMenu->clear();

    // NOTE: this is done so the accounts button text is not set to the accounts menu title
    QMenu* accountsButtonMenu = ui->actionAccountsButton->menu();
    if (accountsButtonMenu) {
        accountsButtonMenu->clear();
    } else {
        accountsButtonMenu = new QMenu(this);
        ui->actionAccountsButton->setMenu(accountsButtonMenu);
    }

    auto accounts = APPLICATION->accounts();
    MinecraftAccountPtr defaultAccount = accounts->defaultAccount();

    bool canChangeSkin = defaultAccount && (defaultAccount->accountType() == AccountType::MSA) && !defaultAccount->isActive();
    ui->actionManageSkins->setEnabled(canChangeSkin);

    QString active_profileId = "";
    if (defaultAccount) {
        // this can be called before accountMenuButton exists
        if (ui->actionAccountsButton) {
            auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
            ui->actionAccountsButton->setText(profileLabel);
        }
    }

    QActionGroup* accountsGroup = new QActionGroup(this);

    if (accounts->count() <= 0) {
        ui->actionNoAccountsAdded->setEnabled(false);
        ui->accountsMenu->addAction(ui->actionNoAccountsAdded);
    } else {
        // TODO: Nicer way to iterate?
        for (int i = 0; i < accounts->count(); i++) {
            MinecraftAccountPtr account = accounts->at(i);
            auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
            QAction* action = new QAction(profileLabel, this);
            action->setData(i);
            action->setCheckable(true);
            action->setActionGroup(accountsGroup);
            if (defaultAccount == account) {
                action->setChecked(true);
            }

            auto face = account->getFace();
            if (!face.isNull()) {
                action->setIcon(face);
            } else {
                action->setIcon(NovaIcons::icon("user"));
            }

            const int highestNumberKey = 9;
            if (i < highestNumberKey) {
                action->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i + 1)));
            }

            ui->accountsMenu->addAction(action);
            connect(action, &QAction::triggered, this, &MainWindow::changeActiveAccount);
        }
    }

    ui->accountsMenu->addSeparator();

    ui->actionNoDefaultAccount->setData(-1);
    ui->actionNoDefaultAccount->setChecked(!defaultAccount);
    ui->actionNoDefaultAccount->setActionGroup(accountsGroup);

    ui->accountsMenu->addAction(ui->actionNoDefaultAccount);

    connect(ui->actionNoDefaultAccount, &QAction::triggered, this, &MainWindow::changeActiveAccount);

    ui->accountsMenu->addSeparator();
    ui->accountsMenu->addAction(ui->actionManageSkins);
    ui->accountsMenu->addAction(ui->actionManageAccounts);

    accountsButtonMenu->addActions(ui->accountsMenu->actions());
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
    if (!APPLICATION->updaterEnabled()) {
        return;
    }
    ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
    QAction* sAction = (QAction*)sender();

    // Profile's associated Mojang username
    if (sAction->data().typeId() != QMetaType::Int)
        return;

    QVariant action_data = sAction->data();
    bool valid = false;
    int index = action_data.toInt(&valid);
    if (!valid) {
        index = -1;
    }
    auto accounts = APPLICATION->accounts();
    accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
    defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
    repopulateAccountsMenu();

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

    // FIXME: this needs adjustment for MSA
    if (account && account->profileName() != "") {
        auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
        auto face = account->getFace();
        if (face.isNull()) {
            ui->actionAccountsButton->setIcon(NovaIcons::icon("user"));
        } else {
            ui->actionAccountsButton->setIcon(face);
        }
        return;
    }

    // Set the icon to the "no account" icon.
    ui->actionAccountsButton->setIcon(NovaIcons::icon("user"));
    ui->actionAccountsButton->setText(tr("Accounts"));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == view) {
        if (ev->type() == QEvent::KeyPress) {
            secretEventFilter->input(ev);
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                    /*
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activateInstance(m_selectedInstance);
                    return true;
                    */
                case Qt::Key_Delete:
                    on_actionDeleteInstance_triggered();
                    return true;
                case Qt::Key_F5:
                    refreshInstances();
                    return true;
                case Qt::Key_F2:
                    on_actionRenameInstance_triggered();
                    return true;
                default:
                    break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
    if (m_newsChecker->isLoadingNews()) {
        ui->newsLabel->setText(tr("Loading news..."));
        ui->newsLabel->setEnabled(false);
        ui->actionMoreNews->setVisible(false);
    } else {
        QList<NewsEntryPtr> entries = m_newsChecker->getNewsEntries();
        if (entries.length() > 0) {
            ui->newsLabel->setText(entries[0]->title);
            ui->newsLabel->setEnabled(true);
            ui->actionMoreNews->setVisible(true);
        } else {
            ui->newsLabel->setText(tr("No news available."));
            ui->newsLabel->setEnabled(false);
            ui->actionMoreNews->setVisible(false);
        }
    }
}

QList<int> stringToIntList(const QString& string)
{
    QStringList split = string.split(',', Qt::SkipEmptyParts);
    QList<int> out;
    for (int i = 0; i < split.size(); ++i) {
        out.append(split.at(i).toInt());
    }
    return out;
}
QString intListToString(const QList<int>& list)
{
    QStringList slist;
    for (int i = 0; i < list.size(); ++i) {
        slist.append(QString::number(list.at(i)));
    }
    return slist.join(',');
}

void MainWindow::onCatToggled(bool state)
{
    setCatBackground(state);
    APPLICATION->settings()->set("TheCat", state);
}

void MainWindow::setCatBackground(bool enabled)
{
    view->setPaintCat(enabled);
    view->viewport()->repaint();
}

void MainWindow::updateCatState()
{
    SettingsObject* settings = APPLICATION->settings();
    const bool catEnabled = settings->get("EnableCat").toBool();
    bool catVisible = settings->get("TheCat").toBool();
    if (!catEnabled && catVisible) {
        settings->set("TheCat", false);
        catVisible = false;
    }

    ui->actionCAT->setVisible(catEnabled);
    ui->actionCAT->setChecked(catVisible);
    setCatBackground(catVisible);
}

void MainWindow::runModalTask(Task* task)
{
    connect(task, &Task::failed, this,
            [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task, &Task::succeeded, this, [this, task]() {
        QStringList warnings = task->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    ProgressDialog loadDialog(this);
    loadDialog.setSkipButton(true, tr("Abort"));
    loadDialog.execWithTask(task);
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));
    runModalTask(task.get());
}

void MainWindow::on_actionCopyInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
    if (!copyInstDlg.exec())
        return;

    auto copyTask = new InstanceCopyTask(m_selectedInstance, copyInstDlg.getChosenOptions());
    copyTask->setName(copyInstDlg.instName());
    copyTask->setGroup(copyInstDlg.instGroup());
    copyTask->setIcon(copyInstDlg.iconKey());
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(copyTask));
    runModalTask(task.get());
}

void MainWindow::addInstance(const QString& url, const QMap<QString, QString>& extra_info)
{
    QString groupName;
    do {
        QObject* obj = sender();
        if (!obj)
            break;
        QAction* action = qobject_cast<QAction*>(obj);
        if (!action)
            break;
        auto map = action->data().toMap();
        if (!map.contains("group"))
            break;
        groupName = map["group"].toString();
    } while (0);

    if (groupName.isEmpty()) {
        groupName = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    }

    NewInstanceDialog newInstDlg(groupName, url, extra_info, this);
    if (!newInstDlg.exec())
        return;

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", newInstDlg.instGroup());
    APPLICATION->settings()->set("LastUsedInstDirForNewInstance", newInstDlg.instDir());

    InstanceTask* creationTask = newInstDlg.extractTask();
    if (creationTask) {
        instanceFromInstanceTask(creationTask);
    }
}

void MainWindow::on_actionAddInstance_triggered()
{
    addInstance();
}

void MainWindow::processURLs(QList<QUrl> urls)
{
    // NOTE: This loop only processes one dropped file!
    for (auto& url : urls) {
        if (url.isEmpty() || url.toString().trimmed().isEmpty())
            continue;

        qDebug() << "Processing" << url;

        // The isLocalFile() check below doesn't work as intended without an explicit scheme.
        if (url.scheme().isEmpty())
            url.setScheme("file");

        ModPlatform::IndexedVersion version;
        QMap<QString, QString> extra_info;
        QUrl local_url;
        if (!url.isLocalFile()) {  // download the remote resource and identify
            if (url.scheme().compare("modrinth", Qt::CaseInsensitive) == 0) {
                const auto packId = ModrinthAPI::getModpackIdFromUrl(url);
                if (!packId.isEmpty()) {
                    extra_info.insert("pack_id", packId);
                    addInstance(url.toString(), extra_info);
                } else {
                    CustomMessageBox::selectable(this, tr("Error"),
                                                 tr("Unsupported Modrinth link.\n\n%1 currently only supports modpack links such as "
                                                    "modrinth://modpack/fabulously-optimized.")
                                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                                 QMessageBox::Critical)
                        ->show();
                }
                continue;
            }

            const bool isExternalURLImport = (url.host().toLower() == "import") || (url.path().startsWith("/import", Qt::CaseInsensitive));

            QUrl dl_url;
            if (url.scheme() == "curseforge" || (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && url.host() == "install")) {
                // need to find the download link for the modpack / resource
                // format of url curseforge://install?addonId=IDHERE&fileId=IDHERE
                // format of url binaryname://install?platform=curseforge&addonId=IDHERE&fileId=IDHERE
                QUrlQuery query(url);

                // check if this is a binaryname:// url
                if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) {
                    // check this is an curseforge platform request
                    if (query.queryItemValue("platform").toLower() != "curseforge") {
                        qDebug() << "Invalid mod distribution platform:" << query.queryItemValue("platform");
                        continue;
                    }
                }

                if (query.allQueryItemValues("addonId").isEmpty() || query.allQueryItemValues("fileId").isEmpty()) {
                    qDebug() << "Invalid curseforge link:" << url;
                    continue;
                }

                auto addonId = query.allQueryItemValues("addonId")[0];
                auto fileId = query.allQueryItemValues("fileId")[0];

                extra_info.insert("pack_id", addonId);
                extra_info.insert("pack_version_id", fileId);

                auto [job, array] = FlameAPI::getFile(addonId, fileId);

                connect(job.get(), &Task::failed, this, [this](const QString& reason) {
                    CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
                });
                connect(job.get(), &Task::succeeded, this, [this, array, addonId, fileId, &dl_url, &version] {
                    qDebug() << "Returned CFURL Json:\n" << array->toStdString().c_str();
                    auto doc = Json::requireDocument(*array);
                    if (!doc) {
                        CustomMessageBox::selectable(this, tr("Error"), doc.error(), QMessageBox::Critical)->show();
                        return;
                    }
                    auto data = doc->object()["data"].toObject();
                    // No way to find out if it's a mod or a modpack before here
                    // And also we need to check if it ends with .zip, instead of any better way
                    auto versionRes = FlameMod::loadIndexedPackVersion(data);
                    if (!versionRes) {
                        CustomMessageBox::selectable(this, tr("Error"), versionRes.error(), QMessageBox::Critical)->show();
                        return;
                    }
                    version = versionRes.value();
                    auto fileName = version.fileName;

                    // Have to use ensureString then use QUrl to get proper url encoding
                    dl_url = QUrl(version.downloadUrl);
                    if (!dl_url.isValid()) {
                        CustomMessageBox::selectable(
                            this, tr("Error"),
                            tr("The modpack, mod, or resource %1 is blocked for third-parties! Please download it manually.").arg(fileName),
                            QMessageBox::Critical)
                            ->show();
                        return;
                    }
                });

                {  // drop stack
                    ProgressDialog dlUrlDialod(this);
                    dlUrlDialod.setSkipButton(true, tr("Abort"));
                    dlUrlDialod.execWithTask(job.get());
                }

            } else if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && !isExternalURLImport) {
                QVariantMap receivedData;
                const QUrlQuery query(url.query());
                const auto items = query.queryItems();
                for (auto it = items.begin(), end = items.end(); it != end; ++it)
                    receivedData.insert(it->first, it->second);
                emit APPLICATION->oauthReplyRecieved(receivedData);
                continue;
            } else if ((url.scheme() == "prismlauncher" || url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) && isExternalURLImport) {
                // PrismLauncher URL protocol modpack import
                // works for any prism fork
                // preferred import format: prismlauncher://import?url=ENCODED
                const auto host = url.host().toLower();
                const auto path = url.path();

                QString encodedTarget;

                {
                    QUrlQuery query(url);
                    const auto values = query.allQueryItemValues("url");
                    if (!values.isEmpty()) {
                        encodedTarget = values.first();
                    }
                }

                // alternative import format: prismlauncher://import/ENCODED
                if (encodedTarget.isEmpty()) {
                    QString p = path;

                    if (p.startsWith("/import/", Qt::CaseInsensitive)) {
                        p = p.mid(QString("/import/").size());
                    } else if (host == "import" && p.startsWith("/")) {
                        p = p.mid(1);
                    }

                    if (!p.isEmpty() && p != "/import") {
                        encodedTarget = p;
                    }
                }

                if (encodedTarget.isEmpty()) {
                    CustomMessageBox::selectable(this, tr("Error"), tr("Invalid import link: missing 'url' parameter."),
                                                 QMessageBox::Critical)
                        ->show();
                    continue;
                }

                const QString decodedStr = QUrl::fromPercentEncoding(encodedTarget.toUtf8()).trimmed();

                QUrl target = QUrl::fromUserInput(decodedStr);

                // Validate: only allow http(s)
                if (!target.isValid() || (target.scheme() != "https" && target.scheme() != "http")) {
                    CustomMessageBox::selectable(this, tr("Error"), tr("Invalid import link: URL must be http(s)."), QMessageBox::Critical)
                        ->show();
                    continue;
                }

                const auto res = QMessageBox::question(
                    this, tr("Install modpack"),
                    tr("Do you want to download and import a modpack from:\n%1\n\nURL:\n%2").arg(target.host(), target.toString()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (res != QMessageBox::Yes) {
                    continue;
                }

                dl_url = target;
            } else {
                dl_url = url;
            }

            if (!dl_url.isValid()) {
                continue;  // no valid url to download this resource
            }

            const QString path = dl_url.host() + '/' + dl_url.path();
            auto entry = APPLICATION->metacache()->resolveEntry("general", path);
            entry->setStale(true);
            auto dl_job = unique_qobject_ptr<NetJob>(new NetJob(tr("Modpack download"), APPLICATION->network()));
            dl_job->addNetAction(Net::ApiRequest::makeCached(dl_url, entry));
            auto archivePath = entry->getFullPath();

            bool dl_success = false;
            connect(dl_job.get(), &Task::failed, this,
                    [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            connect(dl_job.get(), &Task::succeeded, this, [&dl_success] { dl_success = true; });

            {  // drop stack
                ProgressDialog dlUrlDialod(this);
                dlUrlDialod.setSkipButton(true, tr("Abort"));
                dlUrlDialod.execWithTask(dl_job.get());
            }

            if (!dl_success) {
                continue;  // no local file to identify
            }
            local_url = QUrl::fromLocalFile(archivePath);

        } else {
            local_url = url;
        }

        auto localFileName = QDir::toNativeSeparators(local_url.toLocalFile());
        QFileInfo localFileInfo(localFileName);

        if (localFileName.isEmpty() || !localFileInfo.exists()) {
            qDebug() << "Ignoring invalid path" << localFileName;
            continue;
        }

        auto type = ResourceUtils::identify(localFileInfo);

        if (ModPlatform::ResourceTypeUtils::g_VALID_RESOURCES.count(type) == 0) {  // probably instance/modpack
            addInstance(localFileName, extra_info);
            continue;
        }

        if (APPLICATION->instances()->count() <= 0) {
            CustomMessageBox::selectable(this, tr("No instance!"),
                                         tr("No instance available to add the resource to.\nPlease create a new instance before "
                                            "attempting to install this resource again."),
                                         QMessageBox::Critical)
                ->show();
            continue;
        }
        ImportResourceDialog dlg(localFileName, type, this);

        if (dlg.exec() != QDialog::Accepted)
            continue;

        qDebug() << "Adding resource" << localFileName << "to" << dlg.selectedInstanceKey;

        auto inst = APPLICATION->instances()->getInstanceById(dlg.selectedInstanceKey);
        auto minecraftInst = inst;

        switch (type) {
            case ModPlatform::ResourceType::ResourcePack:
                minecraftInst->resourcePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::TexturePack:
                minecraftInst->texturePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::DataPack:
                qWarning() << "Importing of Data Packs not supported at this time. Ignoring" << localFileName;
                break;
            case ModPlatform::ResourceType::Mod:
                minecraftInst->loaderModList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::ShaderPack:
                minecraftInst->shaderPackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::World:
                minecraftInst->worldList()->installWorld(localFileInfo);
                break;
            case ModPlatform::ResourceType::Unknown:
            default:
                qDebug() << "Can't Identify" << localFileName << "Ignoring it.";
                break;
        }
    }
}

void MainWindow::on_actionREDDIT_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionMATRIX_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.MATRIX_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
    if (!m_selectedInstance)
        return;

    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_selectedInstance->iconKey());
    if (dlg.result() == QDialog::Accepted) {
        m_selectedInstance->setIconKey(dlg.selectedIconKey);
        auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
        ui->actionChangeInstIcon->setIcon(icon);
        ui->novaInstanceIcon->setIcon(icon);
    }
}

void MainWindow::iconUpdated(QString icon)
{
    if (icon == m_currentInstIcon) {
        auto new_icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
        ui->actionChangeInstIcon->setIcon(new_icon);
        ui->novaInstanceIcon->setIcon(new_icon);
    }
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
    m_currentInstIcon = new_icon;
    auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
    ui->actionChangeInstIcon->setIcon(icon);
    ui->novaInstanceIcon->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
    if (id.isNull())
        return;
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        QModelIndex selectionIndex = proxymodel->mapFromSource(index);
        // the instance may be hidden by the search filter
        if (!selectionIndex.isValid()) {
            return;
        }
        view->selectionModel()->setCurrentIndex(selectionIndex, QItemSelectionModel::ClearAndSelect);
        updateStatusCenter();
    }
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
    if (!m_selectedInstance)
        return;

    InstanceId instId = m_selectedInstance->id();
    QString src(APPLICATION->instances()->getInstanceGroup(instId));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(src);
    bool ok = false;
    QString dst = QInputDialog::getItem(this, tr("Group name"), tr("Enter a new group name."), groups, index, true, &ok);
    dst = dst.simplified();

    if (ok) {
        APPLICATION->instances()->setInstanceGroup(instId, dst);
    }
}

void MainWindow::deleteGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    const int reply = QMessageBox::question(this, tr("Delete group"), tr("Are you sure you want to delete the group '%1'?").arg(group),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        APPLICATION->instances()->deleteGroup(group);
}

void MainWindow::renameGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    QString name = QInputDialog::getText(this, tr("Rename group"), tr("Enter a new group name."), QLineEdit::Normal, group);
    name = name.simplified();
    if (name.isNull() || name == group)
        return;

    const bool empty = name.isEmpty();
    const bool duplicate = APPLICATION->instances()->getGroups().contains(name, Qt::CaseInsensitive) && group.toLower() != name.toLower();

    if (empty || duplicate) {
        QMessageBox::warning(this, tr("Cannot rename group"), empty ? tr("Cannot set empty name.") : tr("Group already exists. :/"));
        return;
    }

    APPLICATION->instances()->renameGroup(group, name);
}

void MainWindow::undoTrashInstance()
{
    if (!APPLICATION->instances()->undoTrashInstance())
        QMessageBox::warning(
            this, tr("Failed to undo trashing instance"),
            tr("Some instances and shortcuts could not be restored.\nPlease check your trashbin to manually restore them."));
    ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
    DesktopServices::openPath(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
    QString str = APPLICATION->settings()->get("InstanceDir").toString();
    DesktopServices::openPath(str);
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewCatPackFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getCatPacksFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
    DesktopServices::openPath("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->javaPath(), true);
}

void MainWindow::refreshInstances()
{
    APPLICATION->instances()->loadList();
}

void MainWindow::checkForUpdates()
{
    if (APPLICATION->updaterEnabled()) {
        APPLICATION->triggerUpdateCheck();
    } else {
        qWarning() << "Updater not set up. Cannot check for updates.";
    }
}

void MainWindow::on_actionSettings_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "global-settings");
}

void MainWindow::globalSettingsClosed()
{
    proxymodel->invalidate();
    proxymodel->sort(0);
    {
        QSignalBlocker blocker(ui->novaSort);
        ui->novaSort->setCurrentIndex(std::max(0, ui->novaSort->findData(APPLICATION->settings()->get("InstSortMode").toString())));
    }
    updateMainToolBar();
    updateLaunchButton();
    updateThemeMenu();
    updateStatusCenter();
    updateCatState();
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but Prism Launcher exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    update();
}

void MainWindow::on_actionEditInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    if (m_selectedInstance->canEdit()) {
        APPLICATION->showInstanceWindow(m_selectedInstance);
    } else {
        CustomMessageBox::selectable(this, tr("Instance not editable"),
                                     tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."),
                                     QMessageBox::Critical)
            ->show();
    }
}

void MainWindow::on_actionManageSkins_triggered()
{
    auto account = APPLICATION->accounts()->defaultAccount();

    if (account && (account->accountType() == AccountType::MSA) && !account->isActive()) {
        SkinManageDialog dialog(this, account);
        dialog.exec();
    }
}

void MainWindow::on_actionManageAccounts_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "accounts");
}

void MainWindow::on_actionReportBug_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionClearMetadata_triggered()
{
    // This if contains side effects!
    if (!APPLICATION->metacache()->evictAll()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Metadata cache clear Failed!\nTo clear the metadata cache manually, press Folders -> View "
                                        "Launcher Root Folder, and after closing the launcher delete the folder named \"meta\"\n"),
                                     QMessageBox::Warning)
            ->show();
    }

    APPLICATION->metacache()->SaveNow();
}

#ifdef Q_OS_MAC
void MainWindow::on_actionAddToPATH_triggered()
{
    auto binaryPath = APPLICATION->applicationFilePath();
    auto targetPath = QString("/usr/local/bin/%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    qDebug() << "Symlinking" << binaryPath << "to" << targetPath;

    QStringList args;
    args << "-e";
    args << QString("do shell script \"mkdir -p /usr/local/bin && ln -sf '%1' '%2'\" with administrator privileges")
                .arg(binaryPath, targetPath);
    auto outcome = QProcess::execute("/usr/bin/osascript", args);
    if (!outcome) {
        QMessageBox::information(this, tr("Successfully added %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 tr("%1 was successfully added to your PATH. You can now start it by running `%2`.")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.LAUNCHER_APP_BINARY_NAME));
    } else {
        QMessageBox::critical(this, tr("Failed to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                              tr("An error occurred while trying to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}
#endif

void MainWindow::on_actionOpenWiki_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.WIKI_URL));
}

void MainWindow::on_actionMoreNews_triggered()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.exec();
}

void MainWindow::newsButtonClicked()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.toggleArticleList();
    news_dialog.exec();
}

void MainWindow::onCatChanged(int)
{
    setCatBackground(APPLICATION->settings()->get("TheCat").toBool());
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
    if (!m_selectedInstance) {
        return;
    }

    if (m_selectedInstance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Cannot Delete Running Instance"),
                                     tr("The selected instance is currently running and cannot be deleted. Please stop the instance before "
                                        "attempting to delete it."),
                                     QMessageBox::Warning, QMessageBox::Ok)
            ->exec();
        return;
    }
    auto id = m_selectedInstance->id();

    QString shortcutStr;
    auto shortcuts = m_selectedInstance->shortcuts();
    if (!shortcuts.isEmpty())
        shortcutStr = tr(" and its %n registered shortcut(s)", "", shortcuts.size());
    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\"%2.\n"
                                                    "This may be permanent and will completely delete the instance.\n\n"
                                                    "Are you sure?")
                                                     .arg(m_selectedInstance->name(), shortcutStr),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!checkLinkedInstances(id, this, tr("Deleting")))
        return;

    if (APPLICATION->instances()->trashInstance(id)) {
        ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
    } else {
        APPLICATION->instances()->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    selectionBad();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
    if (m_selectedInstance) {
        ExportInstanceDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceMrPack_triggered()
{
    if (m_selectedInstance) {
        ExportPackDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceFlamePack_triggered()
{
    if (m_selectedInstance) {
        if (auto cmp = m_selectedInstance->getPackProfile()->getComponent("net.minecraft");
            cmp && cmp->getVersionFile() && cmp->getVersionFile()->type == "snapshot") {
            QMessageBox msgBox(this);
            msgBox.setText("Snapshots are currently not supported by CurseForge modpacks.");
            msgBox.exec();
            return;
        }
        ExportPackDialog dlg(m_selectedInstance, this, ModPlatform::ResourceProvider::FLAME);
        dlg.exec();
    }
}

void MainWindow::on_actionRenameInstance_triggered()
{
    if (m_selectedInstance) {
        view->edit(view->currentIndex());
    }
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
    if (m_selectedInstance) {
        QString str = m_selectedInstance->instanceRoot();
        DesktopServices::openPath(QFileInfo(str));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
    if (!index.isValid())
        return;
    QString id = index.data(InstanceList::InstanceIDRole).toString();
    MinecraftInstance* inst = APPLICATION->instances()->getInstanceById(id);
    if (!inst)
        return;

    if (APPLICATION->settings()->get("EditInstanceOnDoubleClick").toBool()) {
        if (inst->canEdit()) {
            APPLICATION->showInstanceWindow(inst);
        } else {
            CustomMessageBox::selectable(
                this, tr("Instance not editable"),
                tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."), QMessageBox::Critical)
                ->show();
        }
        return;
    }
    APPLICATION->launch(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
    if (m_selectedInstance && !m_selectedInstance->isRunning()) {
        APPLICATION->launch(m_selectedInstance);
    }
}

void MainWindow::on_actionKillInstance_triggered()
{
    if (m_selectedInstance && m_selectedInstance->isRunning()) {
        APPLICATION->kill(m_selectedInstance);
    }
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
    if (!m_selectedInstance)
        return;

    CreateShortcutDialog shortcutDlg(m_selectedInstance, this);
    if (!shortcutDlg.exec())
        return;
    shortcutDlg.createShortcut();
}

void MainWindow::taskEnd()
{
    QObject* sender = QObject::sender();
    if (sender == m_versionLoadTask)
        m_versionLoadTask = NULL;

    sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
    connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
    connect(task, &Task::failed, this, &MainWindow::taskEnd);
    task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        // the search filter hides instances, that shouldn't forget the selection
        if (!m_filtering) {
            APPLICATION->settings()->set("SelectedInstance", QString());
        }
        selectionBad();
        return;
    }
    if (m_selectedInstance) {
        disconnect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        disconnect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    }
    QString id = current.data(InstanceList::InstanceIDRole).toString();
    m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
    if (m_selectedInstance) {
        m_filterSelection = m_selectedInstance->id();
        setInstanceActionsEnabled(true);
        ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch());

        ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
        ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
        updateStatusCenter();
        updateInstanceToolIcon(m_selectedInstance->iconKey());

        updateLaunchButton();
        updateInspector();

        APPLICATION->settings()->set("SelectedInstance", m_selectedInstance->id());

        connect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        connect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    } else {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
}

void MainWindow::instanceSelectRequest(QString id)
{
    setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto current = view->selectionModel()->currentIndex();
    QItemSelection test(topLeft, bottomRight);
    if (test.contains(current)) {
        instanceChanged(current, current);
    }
}

void MainWindow::selectionBad()
{
    // start by reseting everything...
    m_selectedInstance = nullptr;
    m_statusLeft->setText(tr("No instance selected"));

    statusBar()->clearMessage();
    setInstanceActionsEnabled(false);
    ui->actionLaunchInstance->setEnabled(false);
    ui->actionKillInstance->setEnabled(false);
    updateLaunchButton();
    updateInstanceToolIcon("grass");
    updateInspector();

    // ...and then see if we can enable the previously selected instance
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());
}

void MainWindow::checkInstancePathForProblems()
{
    QString instanceFolder = APPLICATION->settings()->get("InstanceDir").toString();
    if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'!\' and this is known to cause Java problems!"));
        warning.setInformativeText(tr("You have now two options: <br/>"
                                      " - change the instance folder in the settings <br/>"
                                      " - move this installation of %1 to a different folder")
                                       .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
    auto tempFolderText =
        tr("This is a problem: <br/>"
           " - The launcher will likely be deleted without warning by the operating system <br/>"
           " - close the launcher now and extract it to a real location, not a temporary folder");
    QString pathfoldername = QDir(instanceFolder).absolutePath();
    if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'Rar$\' - that means you haven't extracted the launcher archive!"));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    } else if (pathfoldername.startsWith(QDir::tempPath()) || pathfoldername.contains("/TempState/")) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder is in a temporary folder: \'%1\'!").arg(QDir::tempPath()));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
}

void MainWindow::updateStatusCenter()
{
    m_statusCenter->setVisible(APPLICATION->settings()->get("ShowGlobalGameTime").toBool());
    int64_t timePlayed = APPLICATION->playtimeSettings()->get("TotalPlayTime").toLongLong();
    if (timePlayed > 0) {
        m_statusCenter->setText(
            tr("Total playtime: %1")
                .arg(Time::prettifyDuration(timePlayed, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
}
// "Instance actions" are actions that require an instance to be selected (i.e. "new instance" is not here)
// Actions that also require other conditions (e.g. a running instance) won't be changed.
void MainWindow::setInstanceActionsEnabled(bool enabled)
{
    ui->actionRenameInstance->setEnabled(enabled);
    ui->actionChangeInstIcon->setEnabled(enabled);
    ui->actionEditInstance->setEnabled(enabled);
    ui->actionChangeInstGroup->setEnabled(enabled);
    ui->actionViewSelectedInstFolder->setEnabled(enabled);
    ui->actionExportInstance->setEnabled(enabled);
    ui->actionDeleteInstance->setEnabled(enabled);
    ui->actionCopyInstance->setEnabled(enabled);
    ui->actionCreateInstanceShortcut->setEnabled(enabled);
}

void MainWindow::refreshCurrentInstance()
{
    auto current = view->selectionModel()->currentIndex();
    instanceChanged(current, current);
}
