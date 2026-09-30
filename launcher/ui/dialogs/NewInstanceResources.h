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
#include <QString>
#include <optional>

#include "modplatform/ModIndex.h"

class MinecraftInstance;
class QWidget;

/// Mods, resource packs and shaders picked while creating an instance. They are chosen from the regular download
/// dialogs, opened on a throwaway draft instance with the chosen versions, and downloaded once the instance exists.
namespace NewInstanceResources {

enum class Kind { Mod, ResourcePack, ShaderPack };

struct Entry {
    Kind kind;
    ModPlatform::IndexedPack::Ptr pack;
    ModPlatform::IndexedVersion version;
    bool indexed = true;
    QString downloadReason;
    QString dependentOn;
};

struct Components {
    QString minecraftVersion;
    QString loaderUid;      //!< empty for vanilla
    QString loaderVersion;  //!< empty for vanilla
};

/// opens the download dialog for one kind, with the current choice preselected; the new choice for that kind or nothing if cancelled
std::optional<QList<Entry>> choose(QWidget* parent, Kind kind, const Components& components, const QList<Entry>& current);

/// downloads everything into a freshly created instance, with a progress dialog
void install(QWidget* parent, MinecraftInstance* instance, const QList<Entry>& entries);

QList<Entry> ofKind(const QList<Entry>& entries, Kind kind);

}  // namespace NewInstanceResources
