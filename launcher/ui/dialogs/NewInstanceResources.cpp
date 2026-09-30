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

#include "NewInstanceResources.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTemporaryDir>

#include "Application.h"
#include "FileSystem.h"
#include "ResourceDownloadTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "settings/INISettingsObject.h"
#include "tasks/ConcurrentTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"

namespace NewInstanceResources {

namespace {

ResourceFolderModel* modelFor(MinecraftInstance* instance, Kind kind)
{
    switch (kind) {
        case Kind::Mod:
            return instance->loaderModList();
        case Kind::ResourcePack:
            return instance->resourcePackList();
        case Kind::ShaderPack:
            return instance->shaderPackList();
    }
    return nullptr;
}

ResourceDownload::ResourceDownloadDialog* dialogFor(QWidget* parent, MinecraftInstance* instance, Kind kind)
{
    auto* model = modelFor(instance, kind);
    switch (kind) {
        case Kind::Mod:
            return ResourceDownload::ResourceDownloadDialog::createMod(parent, model, instance);
        case Kind::ResourcePack:
            return ResourceDownload::ResourceDownloadDialog::createResourcePack(parent, model, instance);
        case Kind::ShaderPack:
            return ResourceDownload::ResourceDownloadDialog::createShaderPack(parent, model, instance);
    }
    return nullptr;
}

}  // namespace

QList<Entry> ofKind(const QList<Entry>& entries, Kind kind)
{
    QList<Entry> result;
    for (const auto& entry : entries) {
        if (entry.kind == kind) {
            result.append(entry);
        }
    }
    return result;
}

std::optional<QList<Entry>> choose(QWidget* parent, Kind kind, const Components& components, const QList<Entry>& current)
{
    // the download dialogs filter by the instance's Minecraft version and mod loader, so give them a draft with exactly those
    QTemporaryDir draftDir(FS::PathCombine(QDir::tempPath(), "prixum-new-instance-XXXXXX"));
    if (!draftDir.isValid()) {
        QMessageBox::critical(parent, QObject::tr("Error"),
                              QObject::tr("Couldn't create a temporary folder: %1").arg(draftDir.errorString()));
        return std::nullopt;
    }
    // written like a resolved instance: the pages read the cached versions, which a fresh profile only gets from metadata
    QJsonArray packComponents{ QJsonObject{ { "uid", "net.minecraft" },
                                            { "version", components.minecraftVersion },
                                            { "cachedVersion", components.minecraftVersion },
                                            { "important", true } } };
    if (!components.loaderUid.isEmpty()) {
        packComponents.append(QJsonObject{
            { "uid", components.loaderUid }, { "version", components.loaderVersion }, { "cachedVersion", components.loaderVersion } });
    }
    QFile pack(FS::PathCombine(draftDir.path(), "mmc-pack.json"));
    if (!pack.open(QIODevice::WriteOnly) ||
        pack.write(QJsonDocument(QJsonObject{ { "formatVersion", 1 }, { "components", packComponents } }).toJson()) < 0) {
        QMessageBox::critical(parent, QObject::tr("Error"), QObject::tr("Couldn't create a temporary folder: %1").arg(pack.errorString()));
        return std::nullopt;
    }
    pack.close();

    MinecraftInstance draft(APPLICATION->settings(), std::make_unique<INISettingsObject>(FS::PathCombine(draftDir.path(), "instance.cfg")),
                            draftDir.path());
    draft.setName(QObject::tr("New instance"));
    if (auto loaded = draft.getPackProfile()->reload(Net::Mode::Offline); !loaded) {
        QMessageBox::critical(parent, QObject::tr("Error"), loaded.error());
        return std::nullopt;
    }

    std::unique_ptr<ResourceDownload::ResourceDownloadDialog> dialog(dialogFor(parent, &draft, kind));
    if (!dialog) {
        return std::nullopt;
    }
    // keep what was picked before selected
    for (auto entry : ofKind(current, kind)) {
        dialog->addResource(entry.pack, entry.version, entry.downloadReason, entry.dependentOn);
    }
    if (dialog->exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    QList<Entry> chosen;
    for (const auto& task : dialog->getTasks()) {
        chosen.append({ kind, task->getPack(), task->getVersion(), task->isIndexed(), task->getDownloadReason(), task->getDependentOn() });
    }
    return chosen;
}

void install(QWidget* parent, MinecraftInstance* instance, const QList<Entry>& entries)
{
    if (entries.isEmpty()) {
        return;
    }
    ConcurrentTask tasks(QObject::tr("Download mods and extras"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    for (const auto& entry : entries) {
        tasks.addTask(makeShared<ResourceDownloadTask>(entry.pack, entry.version, modelFor(instance, entry.kind), entry.indexed,
                                                       entry.downloadReason, entry.dependentOn));
    }
    QObject::connect(&tasks, &Task::failed, parent, [parent](const QString& reason) {
        CustomMessageBox::selectable(parent, QObject::tr("Error"),
                                     QObject::tr("The instance was created, but some downloads failed:\n%1").arg(reason),
                                     QMessageBox::Critical)
            ->show();
    });
    QObject::connect(&tasks, &Task::succeeded, parent, [parent, &tasks] {
        if (const auto warnings = tasks.warnings(); !warnings.isEmpty()) {
            CustomMessageBox::selectable(parent, QObject::tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });

    ProgressDialog progress(parent);
    progress.setSkipButton(true, QObject::tr("Abort"));
    progress.execWithTask(&tasks);

    for (auto kind : { Kind::Mod, Kind::ResourcePack, Kind::ShaderPack }) {
        modelFor(instance, kind)->update();
    }
}

}  // namespace NewInstanceResources
