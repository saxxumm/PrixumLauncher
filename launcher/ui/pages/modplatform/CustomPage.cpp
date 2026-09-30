// SPDX-License-Identifier: GPL-3.0-only
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

#include "CustomPage.h"
#include "ui_CustomPage.h"

#include <QListWidget>
#include <QPushButton>
#include <QTabBar>
#include <utility>

#include "Application.h"
#include "Filter.h"
#include "Version.h"
#include "icons/IconList.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/themes/NovaIcons.h"

CustomPage::CustomPage(NewInstanceDialog* dialog, QWidget* parent) : QWidget(parent), m_dialog(dialog), m_ui(new Ui::CustomPage)
{
    m_ui->setupUi(this);
    setupIcons();
    connect(m_ui->versionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedVersion);
    filterChanged();
    for (auto* filter : { m_ui->alphaFilter, m_ui->betaFilter, m_ui->snapshotFilter, m_ui->releaseFilter, m_ui->experimentsFilter }) {
        connect(filter, &QAbstractButton::toggled, this, &CustomPage::filterChanged);
        // chips keep their full text instead of shrinking
        filter->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    connect(m_ui->refreshBtn, &QAbstractButton::clicked, this, &CustomPage::refresh);

    connect(m_ui->loaderVersionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedLoaderVersion);
    for (auto* loader :
         { m_ui->noneFilter, m_ui->neoForgeFilter, m_ui->forgeFilter, m_ui->fabricFilter, m_ui->quiltFilter, m_ui->liteLoaderFilter }) {
        // the buttons are auto-exclusive, only react to the one that got checked
        connect(loader, &QAbstractButton::toggled, this, [this](bool checked) {
            if (checked) {
                loaderFilterChanged();
            }
        });
    }
    connect(m_ui->loaderRefreshBtn, &QAbstractButton::clicked, this, &CustomPage::loaderRefresh);

    connect(m_ui->modsButton, &QAbstractButton::clicked, this, [this] { chooseResources(NewInstanceResources::Kind::Mod); });
    connect(m_ui->resourcePacksButton, &QAbstractButton::clicked, this,
            [this] { chooseResources(NewInstanceResources::Kind::ResourcePack); });
    connect(m_ui->shaderPacksButton, &QAbstractButton::clicked, this, [this] { chooseResources(NewInstanceResources::Kind::ShaderPack); });
    connect(m_ui->removeResourcesButton, &QAbstractButton::clicked, this, &CustomPage::removeSelectedResources);
    // clicking these must not take the default button role away from "Create"
    for (auto* button : findChildren<QPushButton*>()) {
        button->setAutoDefault(false);
    }
    m_ui->resourcesList->setMaximumHeight(120);
    connect(m_ui->resourcesList, &QListWidget::itemSelectionChanged, this,
            [this] { m_ui->removeResourcesButton->setEnabled(!m_ui->resourcesList->selectedItems().isEmpty()); });
    updateResources();
}

void CustomPage::setupIcons()
{
    using NovaIcons::Tint;
    m_ui->refreshBtn->setIcon(NovaIcons::icon("refresh", Tint::Muted));
    m_ui->loaderRefreshBtn->setIcon(NovaIcons::icon("refresh", Tint::Muted));

    // brand icons where the launcher has them
    m_ui->noneFilter->setIcon(NovaIcons::icon("cube"));
    m_ui->neoForgeFilter->setIcon(APPLICATION->icons()->getIcon("neoforged"));
    m_ui->forgeFilter->setIcon(NovaIcons::icon("wrench"));
    m_ui->fabricFilter->setIcon(APPLICATION->icons()->getIcon("fabricmc"));
    m_ui->quiltFilter->setIcon(APPLICATION->icons()->getIcon("quiltmc"));
    m_ui->liteLoaderFilter->setIcon(NovaIcons::icon("sparkles"));
    for (auto* loader :
         { m_ui->noneFilter, m_ui->neoForgeFilter, m_ui->forgeFilter, m_ui->fabricFilter, m_ui->quiltFilter, m_ui->liteLoaderFilter }) {
        loader->setIconSize(QSize(20, 20));
    }

    m_ui->modsButton->setIcon(NovaIcons::icon("puzzle"));
    m_ui->resourcePacksButton->setIcon(NovaIcons::icon("layers"));
    m_ui->shaderPacksButton->setIcon(NovaIcons::icon("sun"));
    m_ui->removeResourcesButton->setIcon(NovaIcons::icon("trash", Tint::Danger));
}

void CustomPage::openedImpl()
{
    if (!m_initialized) {
        auto vlist = APPLICATION->metadataIndex()->get("net.minecraft");
        m_ui->versionList->initialize(vlist.get());
        m_initialized = true;
    } else {
        suggestCurrent();
    }
}

void CustomPage::refresh()
{
    m_ui->versionList->loadList(true);
}

void CustomPage::loaderRefresh()
{
    if (m_ui->noneFilter->isChecked()) {
        return;
    }
    m_ui->loaderVersionList->loadList(true);
}

void CustomPage::filterChanged()
{
    QStringList out;
    if (m_ui->alphaFilter->isChecked()) {
        out << "(alpha)";
    }
    if (m_ui->betaFilter->isChecked()) {
        out << "(beta)";
    }
    if (m_ui->snapshotFilter->isChecked()) {
        out << "(snapshot)";
    }
    if (m_ui->releaseFilter->isChecked()) {
        out << "(release)";
    }
    if (m_ui->experimentsFilter->isChecked()) {
        out << "(experiment)";
    }
    auto regexp = out.join('|');
    m_ui->versionList->setFilter(BaseVersionList::TypeRole, Filters::regexp(QRegularExpression(regexp)));
}

void CustomPage::loaderFilterChanged()
{
    dropOutdatedResources();
    updateResources();

    QString minecraftVersion;
    if (m_selectedVersion) {
        minecraftVersion = m_selectedVersion->descriptor();
    } else {
        m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        m_ui->loaderVersionList->setEmptyString(tr("No Minecraft version is selected."));
        m_ui->loaderVersionList->setEmptyMode(VersionListView::String);
        return;
    }
    if (m_ui->noneFilter->isChecked()) {
        m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        m_ui->loaderVersionList->setEmptyString(tr("No mod loader is selected."));
        m_ui->loaderVersionList->setEmptyMode(VersionListView::String);
        return;
    }
    if (m_ui->neoForgeFilter->isChecked()) {
        m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "net.neoforged";
    } else if (m_ui->forgeFilter->isChecked()) {
        m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "net.minecraftforge";
    } else if (m_ui->fabricFilter->isChecked()) {
        // FIXME: dirty hack because the launcher is unaware of Fabric's dependencies
        if (Version(minecraftVersion) >= Version("1.14")) {  // Fabric/Quilt supported
            m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        } else {                                                                                 // Fabric/Quilt unsupported
            m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // clear list
        }
        m_selectedLoader = "net.fabricmc.fabric-loader";
    } else if (m_ui->quiltFilter->isChecked()) {
        // FIXME: dirty hack because the launcher is unaware of Quilt's dependencies (same as Fabric)
        if (Version(minecraftVersion) >= Version("1.14")) {  // Fabric/Quilt supported
            m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        } else {                                                                                 // Fabric/Quilt unsupported
            m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // clear list
        }
        m_selectedLoader = "org.quiltmc.quilt-loader";
    } else if (m_ui->liteLoaderFilter->isChecked()) {
        m_ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "com.mumfrey.liteloader";
    }

    auto vlist = APPLICATION->metadataIndex()->get(m_selectedLoader);
    m_ui->loaderVersionList->initialize(vlist.get());
    m_ui->loaderVersionList->selectRecommended();
    m_ui->loaderVersionList->setEmptyString(tr("No versions are currently available for Minecraft %1").arg(minecraftVersion));
}

CustomPage::~CustomPage()
{
    delete m_ui;
}

bool CustomPage::shouldDisplay() const
{
    return true;
}

void CustomPage::retranslate()
{
    m_ui->retranslateUi(this);
    updateResources();
}

BaseVersion::Ptr CustomPage::selectedVersion() const
{
    return m_selectedVersion;
}

BaseVersion::Ptr CustomPage::selectedLoaderVersion() const
{
    return m_selectedLoaderVersion;
}

QString CustomPage::selectedLoader() const
{
    return m_selectedLoader;
}

QString CustomPage::selectedLoaderName() const
{
    if (m_ui->neoForgeFilter->isChecked()) {
        return m_ui->neoForgeFilter->text();
    }
    if (m_ui->forgeFilter->isChecked()) {
        return m_ui->forgeFilter->text();
    }
    if (m_ui->fabricFilter->isChecked()) {
        return m_ui->fabricFilter->text();
    }
    if (m_ui->quiltFilter->isChecked()) {
        return m_ui->quiltFilter->text();
    }
    if (m_ui->liteLoaderFilter->isChecked()) {
        return m_ui->liteLoaderFilter->text();
    }
    return QString();
}

void CustomPage::suggestCurrent()
{
    if (!isOpened) {
        return;
    }

    if (!m_selectedVersion) {
        m_dialog->setSuggestedPack();
        return;
    }

    // There isn't a selected version if the version list is empty
    if (m_ui->loaderVersionList->selectedVersion() == nullptr) {
        m_dialog->setSuggestedPack(m_selectedVersion->descriptor(), new VanillaCreationTask(m_selectedVersion));
    } else {
        QString suggestedName = QString("%1 %2").arg(m_selectedVersion->descriptor(), selectedLoaderName());
        m_dialog->setSuggestedPack(suggestedName, new VanillaCreationTask(m_selectedVersion, m_selectedLoader, m_selectedLoaderVersion));
    }
    m_dialog->setSuggestedIcon("default");
    updateSummary();
}

void CustomPage::setSelectedVersion(BaseVersion::Ptr version)
{
    m_selectedVersion = std::move(version);
    suggestCurrent();
    loaderFilterChanged();
}

void CustomPage::setSelectedLoaderVersion(BaseVersion::Ptr version)
{
    m_selectedLoaderVersion = std::move(version);
    suggestCurrent();
    updateResources();
}

QString CustomPage::loaderUid() const
{
    return m_ui->noneFilter->isChecked() ? QString() : m_selectedLoader;
}

void CustomPage::chooseResources(NewInstanceResources::Kind kind)
{
    if (!m_selectedVersion) {
        return;
    }
    NewInstanceResources::Components components{ m_selectedVersion->descriptor(), {}, {} };
    if (!loaderUid().isEmpty() && m_selectedLoaderVersion) {
        components.loaderUid = loaderUid();
        components.loaderVersion = m_selectedLoaderVersion->descriptor();
    }
    auto chosen = NewInstanceResources::choose(this, kind, components, m_resources);
    if (!chosen) {
        return;
    }

    QList<NewInstanceResources::Entry> result;
    for (const auto& entry : m_resources) {
        if (entry.kind != kind) {
            result.append(entry);
        }
    }
    result.append(*chosen);
    m_resources = result;
    m_resourcesVersion = components.minecraftVersion;
    m_resourcesLoader = components.loaderUid;
    m_resourcesNote.clear();
    updateResources();
}

void CustomPage::removeSelectedResources()
{
    QList<int> rows;
    for (auto* item : m_ui->resourcesList->selectedItems()) {
        rows.append(item->data(Qt::UserRole).toInt());
    }
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (auto row : rows) {
        if (row >= 0 && row < m_resources.size()) {
            m_resources.removeAt(row);
        }
    }
    updateResources();
}

void CustomPage::dropOutdatedResources()
{
    const auto version = m_selectedVersion ? m_selectedVersion->descriptor() : QString();
    const auto loader = loaderUid();
    if (!m_resources.isEmpty()) {
        QList<NewInstanceResources::Entry> kept;
        for (const auto& entry : m_resources) {
            // mods follow the loader, everything follows the Minecraft version
            const bool outdated =
                version != m_resourcesVersion || (entry.kind == NewInstanceResources::Kind::Mod && loader != m_resourcesLoader);
            if (!outdated) {
                kept.append(entry);
            }
        }
        if (kept.size() != m_resources.size()) {
            m_resourcesNote = tr("The choice was reset because the version or the mod loader changed.");
        }
        m_resources = kept;
    }
    m_resourcesVersion = version;
    m_resourcesLoader = loader;
}

void CustomPage::updateResources()
{
    using NewInstanceResources::Kind;
    auto iconFor = [](Kind kind) {
        switch (kind) {
            case Kind::Mod:
                return NovaIcons::icon("puzzle", NovaIcons::Tint::Muted);
            case Kind::ResourcePack:
                return NovaIcons::icon("layers", NovaIcons::Tint::Muted);
            case Kind::ShaderPack:
                return NovaIcons::icon("sun", NovaIcons::Tint::Muted);
        }
        return QIcon();
    };

    m_ui->resourcesList->clear();
    int mods = 0;
    int resourcePacks = 0;
    int shaders = 0;
    for (int i = 0; i < m_resources.size(); i++) {
        const auto& entry = m_resources[i];
        const auto& version = entry.version.versionNumber.isEmpty() ? entry.version.version : entry.version.versionNumber;
        auto text = QString("%1  ·  %2").arg(entry.pack->name, version);
        if (entry.downloadReason == "dependency") {
            text += "  ·  " + tr("dependency");
        }
        auto* item = new QListWidgetItem(iconFor(entry.kind), text, m_ui->resourcesList);
        item->setData(Qt::UserRole, i);
        mods += entry.kind == Kind::Mod;
        resourcePacks += entry.kind == Kind::ResourcePack;
        shaders += entry.kind == Kind::ShaderPack;
    }
    if (m_resources.isEmpty()) {
        auto* placeholder = new QListWidgetItem(tr("Nothing chosen yet"), m_ui->resourcesList);
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setData(Qt::UserRole, -1);
    }

    auto label = [](const QString& text, int count) { return count ? QString("%1 (%2)").arg(text).arg(count) : text; };
    m_ui->modsButton->setText(label(tr("&Mods"), mods));
    m_ui->resourcePacksButton->setText(label(tr("&Resource packs"), resourcePacks));
    m_ui->shaderPacksButton->setText(label(tr("S&haders"), shaders));

    const bool hasVersion = m_selectedVersion != nullptr;
    const bool hasLoader = !loaderUid().isEmpty() && m_selectedLoaderVersion != nullptr;
    m_ui->modsButton->setEnabled(hasVersion && hasLoader);
    m_ui->modsButton->setToolTip(hasLoader ? QString() : tr("Choose a mod loader to add mods."));
    m_ui->resourcePacksButton->setEnabled(hasVersion);
    m_ui->shaderPacksButton->setEnabled(hasVersion);
    m_ui->removeResourcesButton->setEnabled(!m_ui->resourcesList->selectedItems().isEmpty());

    m_ui->resourcesSummary->setText(m_resources.isEmpty() ? m_resourcesNote : tr("Downloaded after the instance is created"));
    updateSummary();
}

void CustomPage::updateSummary()
{
    if (!isOpened || !m_selectedVersion) {
        return;
    }
    QStringList parts{ QString("Minecraft %1").arg(m_selectedVersion->descriptor()) };
    if (!loaderUid().isEmpty() && m_selectedLoaderVersion) {
        parts << QString("%1 %2").arg(selectedLoaderName(), m_selectedLoaderVersion->descriptor());
    }
    if (!m_resources.isEmpty()) {
        parts << tr("%1 to download").arg(m_resources.size());
    }
    m_dialog->setSummary(parts.join("  ·  "));
}
