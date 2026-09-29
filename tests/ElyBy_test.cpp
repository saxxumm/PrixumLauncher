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

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QTimeZone>

#include <minecraft/auth/AccountData.h>
#include <minecraft/auth/ElyBy.h>
#include <minecraft/auth/MinecraftAccount.h>

class ElyByTest : public QObject {
    Q_OBJECT

    static QByteArray json(const QJsonObject& object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }

    static QJsonObject object(const QByteArray& body) { return QJsonDocument::fromJson(body).object(); }

    static QString jwt(qint64 expiry)
    {
        const auto payload =
            json({ { "exp", expiry }, { "sub", "ely|1" } }).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        return "eyJhbGciOiJIUzI1NiJ9." + QString::fromLatin1(payload) + ".signature";
    }

   private slots:
    void test_authenticateRequest()
    {
        const auto body = object(ElyBy::authenticateRequest("steve", "secret:123456", "client"));
        QCOMPARE(body["username"].toString(), QString("steve"));
        QCOMPARE(body["password"].toString(), QString("secret:123456"));
        QCOMPARE(body["clientToken"].toString(), QString("client"));
        QCOMPARE(body["requestUser"].toBool(), true);
    }

    void test_refreshRequest()
    {
        const auto body = object(ElyBy::refreshRequest("token", "client"));
        QCOMPARE(body["accessToken"].toString(), QString("token"));
        QCOMPARE(body["clientToken"].toString(), QString("client"));
    }

    void test_urls()
    {
        QCOMPARE(ElyBy::authenticateUrl().toString(), QString("https://authserver.ely.by/auth/authenticate"));
        QCOMPARE(ElyBy::refreshUrl().toString(), QString("https://authserver.ely.by/auth/refresh"));
        QCOMPARE(ElyBy::skinUrl("Steve_1").toString(), QString("https://skinsystem.ely.by/skins/Steve_1.png"));
    }

    void test_errors()
    {
        const auto twoFactor = ElyBy::parseError(
            json({ { "error", "ForbiddenOperationException" }, { "errorMessage", "Account protected with two factor auth." } }));
        QCOMPARE(twoFactor.error, QString("ForbiddenOperationException"));
        QVERIFY(ElyBy::isTwoFactorRequired(twoFactor));
        QVERIFY(!ElyBy::isInvalidCredentials(twoFactor));

        const auto wrongPassword = ElyBy::parseError(
            json({ { "error", "ForbiddenOperationException" }, { "errorMessage", "Invalid credentials. Invalid email or password." } }));
        QVERIFY(ElyBy::isInvalidCredentials(wrongPassword));
        QVERIFY(!ElyBy::isTwoFactorRequired(wrongPassword));

        const auto notJson = ElyBy::parseError("<html>Bad Gateway</html>");
        QVERIFY(notJson.error.isEmpty());
        QVERIFY(notJson.message.isEmpty());
    }

    void test_tokenExpiry()
    {
        QCOMPARE(ElyBy::tokenExpiry(jwt(1900000000)), QDateTime::fromSecsSinceEpoch(1900000000, QTimeZone::UTC));
        QVERIFY(!ElyBy::tokenExpiry("not-a-jwt").isValid());
        QVERIFY(!ElyBy::tokenExpiry("a.b.c").isValid());
    }

    void test_parseAuthResponse()
    {
        AccountData data;
        data.type = AccountType::ElyBy;
        const auto token = jwt(1900000000);
        const auto body =
            json({ { "accessToken", token },
                   { "clientToken", "client" },
                   { "selectedProfile", QJsonObject{ { "id", "ffc8fdc95824509e8a57c99b940fb996" }, { "name", "ErickSkin" } } } });
        QVERIFY(ElyBy::parseAuthResponse(body, data));
        QCOMPARE(data.accessToken(), token);
        QCOMPARE(data.yggdrasilToken.validity, Validity::Certain);
        QCOMPARE(data.yggdrasilToken.notAfter, QDateTime::fromSecsSinceEpoch(1900000000, QTimeZone::UTC));
        QCOMPARE(data.yggdrasilToken.extra["clientToken"].toString(), QString("client"));
        QCOMPARE(data.profileId(), QString("ffc8fdc95824509e8a57c99b940fb996"));
        QCOMPARE(data.profileName(), QString("ErickSkin"));
        QCOMPARE(data.minecraftProfile.skin.url, QString("https://skinsystem.ely.by/skins/ErickSkin.png"));
    }

    void test_parseAuthResponseRejectsIncompleteReplies()
    {
        AccountData data;
        QVERIFY(!ElyBy::parseAuthResponse("", data));
        QVERIFY(!ElyBy::parseAuthResponse(json({ { "accessToken", "token" } }), data));
        QVERIFY(!ElyBy::parseAuthResponse(json({ { "selectedProfile", QJsonObject{ { "id", "1" }, { "name", "a" } } } }), data));
        QVERIFY(data.accessToken().isEmpty());
    }

    void test_javaAgentArgument()
    {
        QCOMPARE(ElyBy::Injector::javaAgentArgument("/data/authlib-injector.jar", ElyBy::Injector::s_target),
                 QString("-javaagent:/data/authlib-injector.jar=ely.by"));
    }

    void test_accountRoundTrip()
    {
        // stored Ely.by accounts come back as Ely.by accounts and never count as a license
        auto account = MinecraftAccount::createElyBy("steve");
        account->accountData()->minecraftEntitlement.ownsMinecraft = true;
        QVERIFY(ElyBy::parseAuthResponse(
            json({ { "accessToken", "token" }, { "selectedProfile", QJsonObject{ { "id", "abc" }, { "name", "Steve" } } } }),
            *account->accountData()));

        auto restored = MinecraftAccount::loadFromJsonV3(account->saveToJson());
        QVERIFY(restored);
        QCOMPARE(restored->accountType(), AccountType::ElyBy);
        QCOMPARE(restored->profileName(), QString("Steve"));
        QCOMPARE(restored->accessToken(), QString("token"));
        QCOMPARE(restored->accountData()->yggdrasilToken.extra["userName"].toString(), QString("steve"));
        QVERIFY(!restored->ownsMinecraft());
    }

    void test_sessionUsesAuthlibInjector()
    {
        auto account = MinecraftAccount::createElyBy("steve");
        QVERIFY(ElyBy::parseAuthResponse(
            json({ { "accessToken", "token" }, { "selectedProfile", QJsonObject{ { "id", "abc" }, { "name", "Steve" } } } }),
            *account->accountData()));
        auto session = std::make_shared<AuthSession>();
        account->fillSession(session);
        QCOMPARE(session->user_type, QString("mojang"));
        QCOMPARE(session->authlibInjector, QString("ely.by"));
        QCOMPARE(session->access_token, QString("token"));

        // offline fallback has no session to redirect
        session->MakeOffline("Steve");
        QVERIFY(session->authlibInjector.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ElyByTest)

#include "ElyBy_test.moc"
