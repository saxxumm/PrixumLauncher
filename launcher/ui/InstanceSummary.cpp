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

#include "InstanceSummary.h"

#include <QCoreApplication>
#include <QList>
#include <utility>

#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

namespace InstanceSummary {

QString versionLine(MinecraftInstance* instance)
{
    auto* profile = instance->getPackProfile();
    QString version = profile->getComponentVersion("net.minecraft");
    version = version.isEmpty() ? QCoreApplication::translate("InstanceSummary", "Unknown version")
                                : QCoreApplication::translate("InstanceSummary", "Minecraft %1").arg(version);
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
    return version;
}

}  // namespace InstanceSummary
