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

#include <QString>
#include <QUrl>

/// What a line typed or pasted into the skin manager points at
namespace SkinSource {

struct Source {
    enum class Kind { None, Url, Player };
    Kind kind = Kind::None;
    /// the texture to download, for Url
    QUrl url;
    /// file name for the downloaded texture, for Url
    QString fileName;
    /// the player whose current skin to fetch, for Player
    QString player;
    /// links from NameMC or Mojang's texture server, worth offering from the clipboard
    bool isSkinLink = false;
};

/**
 * Understands player names, links to NameMC skins (the skin page, the texture, a 3D render), NameMC profiles
 * (they name a player) and direct links to PNG textures. NameMC pages themselves are never fetched, only its
 * texture server.
 */
Source parse(const QString& text);

}  // namespace SkinSource
