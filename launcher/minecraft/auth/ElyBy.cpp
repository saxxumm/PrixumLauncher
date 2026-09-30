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

#include "ElyBy.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include "Application.h"
#include "FileSystem.h"
#include "minecraft/auth/AccountData.h"
#include "net/ChecksumValidator.h"
#include "net/NetJob.h"
#include "net/Request.h"

namespace ElyBy {

namespace {

constexpr auto s_authServer = "https://authserver.ely.by";

// pinned release, the checksum is the one published by authlib-injector for this build
constexpr auto s_injectorVersion = "1.2.8";
constexpr auto s_injectorUrl = "https://github.com/yushijinhun/authlib-injector/releases/download/v1.2.8/authlib-injector-1.2.8.jar";
constexpr auto s_injectorSha256 = "9c7f4343e6c82034958ffb48c14a2cb0c85928be7283103ce17da00c6d5a7b10";

QByteArray toJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace

QUrl authenticateUrl()
{
    return QUrl(QString(s_authServer) + "/auth/authenticate");
}

QUrl refreshUrl()
{
    return QUrl(QString(s_authServer) + "/auth/refresh");
}

QUrl skinUrl(const QString& profileName)
{
    return QUrl("https://skinsystem.ely.by/skins/" + QString::fromLatin1(QUrl::toPercentEncoding(profileName)) + ".png");
}

QByteArray authenticateRequest(const QString& username, const QString& password, const QString& clientToken)
{
    return toJson({ { "username", username }, { "password", password }, { "clientToken", clientToken }, { "requestUser", true } });
}

QByteArray refreshRequest(const QString& accessToken, const QString& clientToken)
{
    return toJson({ { "accessToken", accessToken }, { "clientToken", clientToken }, { "requestUser", true } });
}

Error parseError(const QByteArray& body)
{
    const auto root = QJsonDocument::fromJson(body).object();
    return { root["error"].toString(), root["errorMessage"].toString() };
}

bool isTwoFactorRequired(const Error& error)
{
    return error.message.contains("two factor", Qt::CaseInsensitive);
}

bool isInvalidCredentials(const Error& error)
{
    return error.message.contains("Invalid credentials", Qt::CaseInsensitive);
}

QDateTime tokenExpiry(const QString& accessToken)
{
    const auto parts = accessToken.split('.');
    if (parts.size() != 3) {
        return {};
    }
    const auto payload = QByteArray::fromBase64(parts[1].toLatin1(), QByteArray::Base64UrlEncoding);
    // value() copies, operator[] on the temporary object would leave a dangling reference
    const QJsonValue expiry = QJsonDocument::fromJson(payload).object().value("exp");
    if (!expiry.isDouble()) {
        return {};
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(expiry.toDouble()), QTimeZone::UTC);
}

bool parseAuthResponse(const QByteArray& body, AccountData& data)
{
    const auto document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return false;
    }
    const auto root = document.object();
    const auto accessToken = root["accessToken"].toString();
    const auto profile = root["selectedProfile"].toObject();
    const auto id = profile["id"].toString();
    const auto name = profile["name"].toString();
    if (accessToken.isEmpty() || id.isEmpty() || name.isEmpty()) {
        return false;
    }

    auto& token = data.yggdrasilToken;
    token.token = accessToken;
    token.issueInstant = QDateTime::currentDateTimeUtc();
    token.notAfter = tokenExpiry(accessToken);
    token.validity = Validity::Certain;
    if (const auto clientToken = root["clientToken"].toString(); !clientToken.isEmpty()) {
        token.extra["clientToken"] = clientToken;
    }

    // the name can change on Ely.by, the id stays
    auto& minecraftProfile = data.minecraftProfile;
    minecraftProfile.id = id;
    minecraftProfile.name = name;
    minecraftProfile.skin.url = skinUrl(name).toString();
    minecraftProfile.validity = Validity::Certain;
    return true;
}

namespace Injector {

QString version()
{
    return s_injectorVersion;
}

QString path()
{
    return FS::PathCombine(APPLICATION->dataRoot(), "authlib-injector", QString("authlib-injector-%1.jar").arg(s_injectorVersion));
}

bool isInstalled()
{
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex() == s_injectorSha256;
}

Task::Ptr downloadTask()
{
    auto job = makeShared<NetJob>(QObject::tr("Download authlib-injector"), APPLICATION->network());
    auto request = Net::Request::makeFile(QUrl(s_injectorUrl), path());
    request->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha256, QString(s_injectorSha256)));
    job->addNetAction(request);
    return job;
}

QString javaAgentArgument(const QString& jarPath, const QString& target)
{
    return QString("-javaagent:%1=%2").arg(jarPath, target);
}

}  // namespace Injector

}  // namespace ElyBy
