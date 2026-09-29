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
#include <QString>
#include <QUrl>

#include "tasks/Task.h"

struct AccountData;

/// Ely.by accounts: a Yggdrasil compatible login server with its own skins, used by many servers instead of Mojang's.
/// The game is pointed at it with authlib-injector (https://github.com/yushijinhun/authlib-injector).
namespace ElyBy {

QUrl authenticateUrl();
QUrl refreshUrl();
QUrl skinUrl(const QString& profileName);

/// body of /auth/authenticate, a TOTP code is appended to the password as "password:code"
QByteArray authenticateRequest(const QString& username, const QString& password, const QString& clientToken);
/// body of /auth/refresh
QByteArray refreshRequest(const QString& accessToken, const QString& clientToken);

struct Error {
    QString error;    //!< short name, e.g. ForbiddenOperationException
    QString message;  //!< English description meant for users
};

/// the error object of a rejected request, empty if the body is not one
Error parseError(const QByteArray& body);
bool isTwoFactorRequired(const Error& error);
bool isInvalidCredentials(const Error& error);

/// expiry of a JWT access token, invalid if the token is not a JWT
QDateTime tokenExpiry(const QString& accessToken);

/// stores the token and profile of an authenticate or refresh response, false if the response is incomplete
bool parseAuthResponse(const QByteArray& body, AccountData& data);

namespace Injector {

/// what authlib-injector gets after '=', it discovers the Ely.by API from it
inline constexpr auto s_target = "ely.by";

QString version();
/// where the pinned authlib-injector jar lives in the launcher data folder
QString path();
/// the jar exists and matches the pinned checksum
bool isInstalled();
/// downloads the pinned jar into path()
Task::Ptr downloadTask();
QString javaAgentArgument(const QString& jarPath, const QString& target);

}  // namespace Injector

}  // namespace ElyBy
