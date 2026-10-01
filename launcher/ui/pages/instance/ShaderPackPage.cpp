// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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

#include "ShaderPackPage.h"
#include "ui_ExternalResourcesPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <algorithm>

#include "ModFolderPage.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "ui/themes/NovaIcons.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/dialogs/ResourceUpdateDialog.h"

ShaderPackPage::ShaderPackPage(MinecraftInstance* instance, ShaderPackFolderModel* model, QWidget* parent)
    : ExternalResourcesPage(instance, model, parent), m_model(model)
{
    m_ui->actionDownloadItem->setText(tr("Download Packs"));
    m_ui->actionDownloadItem->setToolTip(tr("Download shader packs from online mod platforms"));
    m_ui->actionDownloadItem->setEnabled(true);
    m_ui->actionsToolbar->insertActionBefore(m_ui->actionAddItem, m_ui->actionDownloadItem);

    connect(m_ui->actionDownloadItem, &QAction::triggered, this, &ShaderPackPage::downloadShaderPack);

    m_ui->actionUpdateItem->setToolTip(tr("Try to check or update all selected shader packs (all shader packs if none are selected)"));
    connect(m_ui->actionUpdateItem, &QAction::triggered, this, &ShaderPackPage::updateShaderPacks);
    m_ui->actionsToolbar->insertActionBefore(m_ui->actionAddItem, m_ui->actionUpdateItem);

    auto* updateMenu = new QMenu(this);

    auto* update = updateMenu->addAction(m_ui->actionUpdateItem->text());
    connect(update, &QAction::triggered, this, &ShaderPackPage::updateShaderPacks);

    updateMenu->addAction(m_ui->actionResetItemMetadata);
    connect(m_ui->actionResetItemMetadata, &QAction::triggered, this, &ShaderPackPage::deleteShaderPackMetadata);

    m_ui->actionUpdateItem->setMenu(updateMenu);

    m_ui->actionChangeVersion->setToolTip(tr("Change a shader pack's version."));
    connect(m_ui->actionChangeVersion, &QAction::triggered, this, &ShaderPackPage::changeShaderPackVersion);
    m_ui->actionsToolbar->insertActionAfter(m_ui->actionUpdateItem, m_ui->actionChangeVersion);

    m_ui->actionsToolbar->insertActionAfter(m_ui->actionChangeVersion, m_ui->actionLockUpdates);
    m_ui->actionsToolbar->insertActionAfter(m_ui->actionLockUpdates, m_ui->actionUnlockUpdates);

    // the game ignores shader packs without Iris or Oculus, say so before someone wonders why nothing changed
    m_loaderNotice = new QFrame(this);
    m_loaderNotice->setObjectName("shaderLoaderNotice");
    auto* noticeLayout = new QHBoxLayout(m_loaderNotice);
    noticeLayout->setContentsMargins(12, 8, 8, 8);
    noticeLayout->setSpacing(10);
    auto* noticeIcon = new QLabel(m_loaderNotice);
    noticeIcon->setPixmap(NovaIcons::icon("sparkles", NovaIcons::Tint::Accent).pixmap(20, 20));
    noticeLayout->addWidget(noticeIcon);
    m_loaderNoticeText = new QLabel(m_loaderNotice);
    m_loaderNoticeText->setWordWrap(true);
    noticeLayout->addWidget(m_loaderNoticeText, 1);
    m_loaderNoticeButton = new QPushButton(m_loaderNotice);
    m_loaderNoticeButton->setIcon(NovaIcons::icon("download"));
    noticeLayout->addWidget(m_loaderNoticeButton);
    connect(m_loaderNoticeButton, &QPushButton::clicked, this, &ShaderPackPage::downloadShaderLoader);
    m_loaderNotice->hide();
    m_ui->pageLayout->insertWidget(0, m_loaderNotice);

    auto* mods = m_instance->loaderModList();
    connect(mods, &ResourceFolderModel::updateFinished, this, [this] {
        m_modsListed = true;
        updateLoaderNotice();
    });
    connect(mods, &ResourceFolderModel::parseFinished, this, &ShaderPackPage::updateLoaderNotice);
}

void ShaderPackPage::openedImpl()
{
    ExternalResourcesPage::openedImpl();
    // the mods may not be listed yet when the mods page was never opened
    m_instance->loaderModList()->update();
    updateLoaderNotice();
}

QString ShaderPackPage::shaderLoader() const
{
    return m_instance->getPackProfile()->getComponent("net.minecraftforge") ? "Oculus" : "Iris";
}

void ShaderPackPage::updateLoaderNotice()
{
    if (!m_modsListed) {
        return;
    }
    // by file name too, the mod id is only known once the jar was read
    const auto mods = m_instance->loaderModList()->allMods();
    const bool installed = std::ranges::any_of(mods, [](Mod* mod) {
        if (!mod->enabled()) {
            return false;
        }
        const auto fileName = mod->fileinfo().fileName().toLower();
        return std::ranges::any_of(std::initializer_list<const char*>{ "iris", "oculus", "optifine" },
                                   [&](const char* loader) { return mod->modId() == loader || fileName.startsWith(loader); });
    });
    const auto loader = shaderLoader();
    m_loaderNoticeText->setText(tr("Shader packs need the %1 mod, without it the game ignores them.").arg(loader));
    m_loaderNoticeButton->setText(tr("Download %1").arg(loader));
    m_loaderNotice->setVisible(!installed);
}

void ShaderPackPage::downloadShaderLoader()
{
    if (!m_container || !m_container->selectPage("mods")) {
        return;
    }
    if (auto* modsPage = dynamic_cast<ModFolderPage*>(m_container->selectedPage())) {
        modsPage->searchOnline(shaderLoader());
    }
}

void ShaderPackPage::downloadShaderPack()
{
    m_downloadDialog = ResourceDownload::ResourceDownloadDialog::createShaderPack(this, m_model, m_instance);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &ShaderPackPage::downloadDialogFinished);

    m_downloadDialog->open();
}

void ShaderPackPage::downloadDialogFinished(int result)
{
    if (result != 0) {
        ConcurrentTask tasks("Download Shader Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(&tasks, &Task::failed, this, [this](const QString& reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
        });
        connect(&tasks, &Task::succeeded, this, [this, &tasks]() {
            QStringList warnings = tasks.warnings();
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
            }
        });

        if (m_downloadDialog) {
            for (auto& task : m_downloadDialog->getTasks()) {
                tasks.addTask(task);
            }
        } else {
            qWarning() << "ResourceDownloadDialog vanished before we could collect tasks!";
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(&tasks);

        m_model->update();
    }
    if (m_downloadDialog) {
        m_downloadDialog->deleteLater();
    }
}

void ShaderPackPage::updateShaderPacks()
{
    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Shader pack updates are unavailable when metadata is disabled!"));
        return;
    }
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response =
            CustomMessageBox::selectable(this, tr("Confirm Update"),
                                         tr("Updating shader packs while the game is running may pack duplication and game crashes.\n"
                                            "The old files may not be deleted as they are in use.\n"
                                            "Are you sure you want to do this?"),
                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();

    auto modsList = m_model->selectedResources(selection);
    bool useAll = modsList.empty();
    if (useAll) {
        modsList = m_model->allResources();
    }

    ResourceUpdateDialog updateDialog(this, m_instance, m_model, modsList, false);
    updateDialog.checkCandidates();

    if (updateDialog.aborted()) {
        return;
    }
    if (updateDialog.noUpdates()) {
        QString message{ tr("'%1' is up-to-date! :)").arg(modsList.front()->name()) };
        if (modsList.size() > 1) {
            if (useAll) {
                message = tr("All shader packs are up-to-date! :)");
            } else {
                message = tr("All selected shader packs are up-to-date! :)");
            }
        }
        CustomMessageBox::selectable(this, tr("Update checker"), message)->exec();
        return;
    }

    if (updateDialog.exec() != 0) {
        ConcurrentTask tasks("Download Shader Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(&tasks, &Task::failed, this, [this](const QString& reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
        });
        connect(&tasks, &Task::succeeded, this, [this, &tasks]() {
            QStringList warnings = tasks.warnings();
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
            }
        });

        for (const auto& task : updateDialog.getTasks()) {
            tasks.addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(&tasks);

        m_model->update();
    }
}

void ShaderPackPage::deleteShaderPackMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
    auto selectionCount = m_model->selectedShaderPacks(selection).length();
    if (selectionCount == 0) {
        return;
    }
    if (selectionCount > 1) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"),
                                                     tr("You are about to remove the metadata for %1 shader packs.\n"
                                                        "Are you sure?")
                                                         .arg(selectionCount),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }

    m_model->deleteMetadata(selection);
}

void ShaderPackPage::changeShaderPackVersion()
{
    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Shader pack updates are unavailable when metadata is disabled!"));
        return;
    }

    const QModelIndexList rows = m_ui->treeView->selectionModel()->selectedRows();

    if (rows.count() != 1) {
        return;
    }

    Resource& resource = m_model->at(m_filterModel->mapToSource(rows[0]).row());

    if (resource.metadata() == nullptr) {
        return;
    }

    m_downloadDialog = ResourceDownload::ResourceDownloadDialog::createShaderPack(this, m_model, m_instance, true);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &ShaderPackPage::downloadDialogFinished);

    m_downloadDialog->setResourceMetadata(resource.metadata());
    m_downloadDialog->open();
}
