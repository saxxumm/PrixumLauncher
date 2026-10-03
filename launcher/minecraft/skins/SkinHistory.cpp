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

#include "SkinHistory.h"

#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace SkinHistory {

namespace {

const QRegularExpression s_hash(QStringLiteral("^[0-9a-f]{32,128}$"));

}  // namespace

QUrl playerUrl(const QString& profileId)
{
    QString id = profileId;
    id.remove('-');
    return QUrl(QStringLiteral("https://api.crafty.gg/api/v2/players/%1").arg(id));
}

QString textureUrl(const QString& hash)
{
    return QStringLiteral("https://textures.minecraft.net/texture/%1").arg(hash);
}

QString hashOfUrl(const QString& url)
{
    const QUrl parsed(url);
    if (parsed.host() != "textures.minecraft.net") {
        return {};
    }
    const QString hash = parsed.fileName();
    return s_hash.match(hash).hasMatch() ? hash : QString();
}

QList<Entry> parsePlayer(const QByteArray& json, QString* error)
{
    auto fail = [error](const QString& reason) {
        if (error) {
            *error = reason;
        }
        return QList<Entry>();
    };
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return fail(QStringLiteral("not JSON: %1").arg(parseError.errorString()));
    }
    const auto root = doc.object();
    if (!root.value("success").toBool()) {
        return fail(root.value("message").toString(QStringLiteral("unknown player")));
    }
    const auto skins = root.value("data").toObject().value("skins").toArray();

    QList<Entry> entries;
    for (const auto& value : skins) {
        const auto skin = value.toObject();
        if (skin.value("hidden").toBool() || skin.value("banned").toBool() || !skin.value("deleted_at").isNull()) {
            continue;
        }
        Entry entry;
        entry.hash = skin.value("hash").toString().toLower();
        if (!s_hash.match(entry.hash).hasMatch()) {
            continue;
        }
        entry.png = QByteArray::fromBase64(skin.value("texture").toString().toLatin1());
        QImage texture;
        // the same sizes the skin list accepts
        if (!texture.loadFromData(entry.png, "PNG") || texture.width() != 64 || (texture.height() != 64 && texture.height() != 32)) {
            continue;
        }
        entry.slim = skin.value("slim").toBool();
        for (const auto* key : { "changed_at", "created_at" }) {
            entry.wornAt = QDateTime::fromString(skin.value(key).toString(), Qt::ISODateWithMs);
            if (entry.wornAt.isValid()) {
                break;
            }
        }
        entries << entry;
    }
    return entries;
}

}  // namespace SkinHistory
