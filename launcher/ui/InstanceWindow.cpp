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

#include "InstanceWindow.h"
#include "Application.h"
#include "ui/InstanceSummary.h"
#include "ui/themes/NovaIcons.h"

#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/widgets/PageContainer.h"

#include "InstancePageProvider.h"

#include "icons/IconList.h"
#include "minecraft/PackProfile.h"

InstanceWindow::InstanceWindow(MinecraftInstance* instance, QWidget* parent) : QMainWindow(parent), m_instance(instance)
{
    setAttribute(Qt::WA_DeleteOnClose);

    auto icon = APPLICATION->icons()->getIcon(m_instance->iconKey());
    QString windowTitle = tr("Console window for ") + m_instance->name();

    // Set window properties
    {
        setWindowIcon(icon);
        setWindowTitle(windowTitle);
    }

    // Add page container
    {
        auto provider = std::make_shared<InstancePageProvider>(m_instance);
        m_container = new PageContainer(provider.get(), "console", this);
        m_container->setParentContainer(this);
        setCentralWidget(m_container);
        setContentsMargins(0, 0, 0, 0);
        createSidebarHeader();
    }

    // Add custom buttons to the page container layout.
    {
        auto horizontalLayout = new QHBoxLayout(this);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 6, 6);

        auto btnHelp = new QPushButton(this);
        btnHelp->setText(tr("Help"));
        horizontalLayout->addWidget(btnHelp);
        connect(btnHelp, &QPushButton::clicked, m_container, &PageContainer::help);

        auto spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        horizontalLayout->addSpacerItem(spacer);

        m_launchButton = new QToolButton(this);
        m_launchButton->setText(tr("&Launch"));
        m_launchButton->setToolTip(tr("Launch the instance"));
        m_launchButton->setPopupMode(QToolButton::MenuButtonPopup);
        m_launchButton->setMinimumWidth(80);  // HACK!!
        m_launchButton->setObjectName("novaPlayButton");
        m_launchButton->setIcon(NovaIcons::icon("play", NovaIcons::Tint::AccentText));
        m_launchButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        horizontalLayout->addWidget(m_launchButton);
        connect(m_launchButton, &QPushButton::clicked, this, [this] { APPLICATION->launch(m_instance); });

        m_restartButton = new QPushButton(this);
        m_restartButton->setText(tr("&Restart"));
        m_restartButton->setToolTip(tr("Restart the running instance"));
        horizontalLayout->addWidget(m_restartButton);
        connect(m_restartButton, &QPushButton::clicked, this, &InstanceWindow::restartInstance);

        m_restartButton->hide();

        m_killButton = new QPushButton(this);
        m_killButton->setText(tr("&Kill"));
        m_killButton->setToolTip(tr("Kill the running instance"));
        m_killButton->setShortcut(QKeySequence(tr("Ctrl+K")));
        m_killButton->setIcon(NovaIcons::icon("stop", NovaIcons::Tint::Danger));
        horizontalLayout->addWidget(m_killButton);
        connect(m_killButton, &QPushButton::clicked, this, [this] { APPLICATION->kill(m_instance); });

        updateButtons();

        m_closeButton = new QPushButton(this);
        m_closeButton->setText(tr("Close"));
        horizontalLayout->addWidget(m_closeButton);
        connect(m_closeButton, &QPushButton::clicked, this, &QMainWindow::close);

        m_container->addButtons(horizontalLayout);

        connect(m_instance, &BaseInstance::profilerChanged, this, &InstanceWindow::updateButtons);
        connect(APPLICATION, &Application::globalSettingsApplied, this, &InstanceWindow::updateButtons);
    }

    // restore window state
    {
        auto base64State = APPLICATION->settings()->get("ConsoleWindowState").toString().toUtf8();
        restoreState(QByteArray::fromBase64(base64State));
        auto base64Geometry = APPLICATION->settings()->get("ConsoleWindowGeometry").toString().toUtf8();
        // the mods page needs room for its toolbar and columns, the size Qt picks on its own is too small
        if (!restoreGeometry(QByteArray::fromBase64(base64Geometry))) {
            if (auto* screen = QGuiApplication::primaryScreen()) {
                resize(QSize(1180, 760).boundedTo(screen->availableSize() * 0.9));
            }
        }
    }

    // set up instance and launch process recognition
    {
        auto launchTask = m_instance->getLaunchTask();
        instanceLaunchTaskChanged(launchTask);
        connect(m_instance, &BaseInstance::launchTaskChanged, this, &InstanceWindow::instanceLaunchTaskChanged);
        connect(m_instance, &BaseInstance::runningStatusChanged, this, &InstanceWindow::runningStateChanged);
    }

    // set up instance destruction detection
    {
        connect(m_instance, &BaseInstance::statusChanged, this, &InstanceWindow::on_instanceStatusChanged);
    }

    // add ourself as the modpack page's instance window
    {
        static_cast<ManagedPackPage*>(m_container->getPage("managed_pack"))->setInstanceWindow(this);
    }

    show();
}

void InstanceWindow::createSidebarHeader()
{
    auto* header = new QFrame(this);
    header->setObjectName("instanceHeader");
    // keeps "Minecraft 1.21.4 · Fabric 0.16.9" on one line
    header->setMinimumWidth(256);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(14, 14, 12, 12);
    layout->setSpacing(10);

    m_headerIcon = new QLabel(header);
    m_headerIcon->setFixedSize(36, 36);
    layout->addWidget(m_headerIcon, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(1);
    m_headerName = new QLabel(header);
    m_headerName->setObjectName("instanceHeaderName");
    m_headerName->setWordWrap(true);
    m_headerVersion = new QLabel(header);
    m_headerVersion->setProperty("novaRole", "muted");
    m_headerVersion->setWordWrap(true);
    text->addWidget(m_headerName);
    text->addWidget(m_headerVersion);
    layout->addLayout(text, 1);
    m_container->setSidebarHeader(header);

    connect(m_instance, &BaseInstance::propertiesChanged, this, &InstanceWindow::updateSidebarHeader);
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, [this](const QString& key) {
        if (key == m_instance->iconKey()) {
            updateSidebarHeader();
        }
    });
    // versions change on the version page
    auto* profile = m_instance->getPackProfile();
    connect(profile, &PackProfile::dataChanged, this, &InstanceWindow::updateSidebarHeader);
    connect(profile, &PackProfile::rowsInserted, this, &InstanceWindow::updateSidebarHeader);
    connect(profile, &PackProfile::rowsRemoved, this, &InstanceWindow::updateSidebarHeader);
    connect(profile, &PackProfile::modelReset, this, &InstanceWindow::updateSidebarHeader);
    updateSidebarHeader();
}

void InstanceWindow::updateSidebarHeader()
{
    m_headerIcon->setPixmap(APPLICATION->icons()->getIcon(m_instance->iconKey()).pixmap(m_headerIcon->size()));
    m_headerName->setText(m_instance->name());
    // wraps between Minecraft and the mod loader, never inside "Fabric 0.16.9"
    auto parts = InstanceSummary::versionLine(m_instance).split(" · ");
    for (auto& part : parts) {
        part.replace(' ', QChar::Nbsp);
    }
    m_headerVersion->setText(parts.join(" · "));
}

void InstanceWindow::on_instanceStatusChanged(BaseInstance::Status, BaseInstance::Status newStatus)
{
    if (newStatus == BaseInstance::Status::Gone) {
        m_doNotSave = true;
        close();
    }
}

void InstanceWindow::updateButtons()
{
    const bool running = m_instance->isRunning();

    m_launchButton->setVisible(!running);
    m_restartButton->setVisible(running);

    m_launchButton->setEnabled(m_instance->canLaunch());
    m_restartButton->setEnabled(running && !m_restartQueued);
    m_killButton->setEnabled(running);

    QMenu* launchMenu = m_launchButton->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    m_instance->populateLaunchMenu(launchMenu);
    m_launchButton->setMenu(launchMenu);
}

void InstanceWindow::instanceLaunchTaskChanged(LaunchTask* proc)
{
    m_proc = proc;
}

void InstanceWindow::runningStateChanged(bool running)
{
    updateButtons();
    m_container->refreshContainer();
    if (running) {
        selectPage("log");
    } else if (m_restartQueued) {
        m_restartQueued = false;
        // Wait until the current launch controller has handled the stopped instance before launching again.
        QTimer::singleShot(0, this, [this] {
            APPLICATION->launch(m_instance);
            updateButtons();
        });
    }
}

void InstanceWindow::restartInstance()
{
    if (!m_instance->isRunning()) {
        return;
    }

    m_restartQueued = APPLICATION->kill(m_instance);
    updateButtons();
}

void InstanceWindow::closeEvent(QCloseEvent* event)
{
    bool proceed = true;
    if (!m_doNotSave) {
        proceed &= m_container->prepareToClose();
    }

    if (!proceed) {
        return;
    }

    APPLICATION->settings()->set("ConsoleWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("ConsoleWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    emit isClosing();
    event->accept();
}

bool InstanceWindow::saveAll()
{
    return m_container->saveAll();
}

QString InstanceWindow::instanceId()
{
    return m_instance->id();
}

bool InstanceWindow::selectPage(QString pageId)
{
    return m_container->selectPage(pageId);
}

void InstanceWindow::refreshContainer()
{
    m_container->refreshContainer();
}

BasePage* InstanceWindow::selectedPage() const
{
    return m_container->selectedPage();
}

bool InstanceWindow::requestClose()
{
    if (m_container->prepareToClose()) {
        close();
        return true;
    }
    return false;
}
