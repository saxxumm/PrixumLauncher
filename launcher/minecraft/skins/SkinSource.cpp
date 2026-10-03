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

#include "SkinSource.h"

#include <QRegularExpression>
#include <QUrlQuery>

namespace SkinSource {

namespace {

const QRegularExpression s_playerName(QStringLiteral("^[A-Za-z0-9_]{1,16}$"));
const QRegularExpression s_nameMcId(QStringLiteral("^[0-9a-f]{16}$"));

Source nameMcSkin(const QString& id)
{
    Source source;
    source.kind = Source::Kind::Url;
    source.url = QUrl(QStringLiteral("https://s.namemc.com/i/%1.png").arg(id));
    source.fileName = QStringLiteral("namemc-%1.png").arg(id);
    source.isSkinLink = true;
    return source;
}

Source player(const QString& name)
{
    Source source;
    if (s_playerName.match(name).hasMatch()) {
        source.kind = Source::Kind::Player;
        source.player = name;
    }
    return source;
}

}  // namespace

Source parse(const QString& text)
{
    const QString input = text.trimmed();
    if (input.isEmpty()) {
        return {};
    }
    if (s_playerName.match(input).hasMatch()) {
        return player(input);
    }
    if (input.contains(QRegularExpression(QStringLiteral("\\s")))) {
        return {};
    }

    const QUrl url = QUrl::fromUserInput(input);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        return {};
    }
    const QString host = url.host().toLower();
    const QStringList path = url.path().split('/', Qt::SkipEmptyParts);

    if (host == "namemc.com" || host.endsWith(".namemc.com")) {
        // namemc.com/skin/<id> and s.namemc.com/i/<id>.png name the same texture
        if (path.size() == 2 && path[0] == "skin" && s_nameMcId.match(path[1]).hasMatch()) {
            return nameMcSkin(path[1]);
        }
        if (path.size() == 2 && path[0] == "i" && path[1].endsWith(".png")) {
            const QString id = path[1].chopped(4);
            if (s_nameMcId.match(id).hasMatch()) {
                return nameMcSkin(id);
            }
        }
        // the 3D renders carry the texture id in the query
        if (!path.isEmpty() && path[0] == "3d") {
            const QString id = QUrlQuery(url).queryItemValue("id");
            if (s_nameMcId.match(id).hasMatch()) {
                return nameMcSkin(id);
            }
        }
        // a profile only names a player, "Notch.1" is NameMC's suffix for the first profile with that name
        if (path.size() >= 2 && path[0] == "profile") {
            return player(path[1].section('.', 0, 0));
        }
        return {};
    }

    Source source;
    source.kind = Source::Kind::Url;
    source.url = url;
    if (host == "textures.minecraft.net" && path.size() == 2 && path[0] == "texture") {
        source.fileName = QStringLiteral("skin-%1.png").arg(path[1].left(12));
        source.isSkinLink = true;
        return source;
    }
    source.fileName = path.isEmpty() ? QStringLiteral("skin.png") : path.last();
    if (!source.fileName.endsWith(".png", Qt::CaseInsensitive)) {
        source.fileName += ".png";
    }
    return source;
}

}  // namespace SkinSource
