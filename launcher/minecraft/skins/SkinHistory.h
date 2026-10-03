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

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

/**
 * The skins a player wore before, as the Crafty public API (crafty.gg) remembers them. Mojang only knows the current
 * skin. Crafty answers with the textures themselves, named by the same hash as Mojang's texture server.
 */
namespace SkinHistory {

struct Entry {
    /// the texture hash of textures.minecraft.net
    QString hash;
    /// the PNG file, ready to be saved
    QByteArray png;
    bool slim = false;
    /// when the player put the skin on, invalid if Crafty does not know
    QDateTime wornAt;
};

/// the player endpoint takes a UUID, which survives name changes
QUrl playerUrl(const QString& profileId);

/// the address of the texture on Mojang's server, the one an account reports while wearing the skin
QString textureUrl(const QString& hash);

/// the hash at the end of a texture address, empty for other addresses
QString hashOfUrl(const QString& url);

/**
 * The skins in a player response, in the order of the response (newest first). Skins the player hid, removed ones and
 * textures that are not skins are left out.
 * @param error set when the response is not a player, for example because Crafty never saw the account
 */
QList<Entry> parsePlayer(const QByteArray& json, QString* error = nullptr);

}  // namespace SkinHistory
