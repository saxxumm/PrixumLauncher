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

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <minecraft/skins/SkinHistory.h>

class SkinHistoryTest : public QObject {
    Q_OBJECT

    static QString png(int width, int height)
    {
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return QString::fromLatin1(data.toBase64());
    }

    static QJsonObject skin(const QString& hash, const QString& texture, bool slim = false)
    {
        return { { "hash", hash },
                 { "texture", texture },
                 { "slim", slim },
                 { "hidden", false },
                 { "banned", false },
                 { "deleted_at", QJsonValue::Null },
                 { "changed_at", "2025-01-13T08:20:32.200+01:00" },
                 { "created_at", "2021-05-31T12:24:00.488+02:00" } };
    }

    static QByteArray player(const QJsonArray& skins)
    {
        return QJsonDocument(QJsonObject{ { "success", true }, { "data", QJsonObject{ { "username", "Dream" }, { "skins", skins } } } })
            .toJson();
    }

   private slots:
    void test_urls()
    {
        QCOMPARE(SkinHistory::playerUrl("069a79f4-44e9-4726-a5be-fca90e38aaf5"),
                 QUrl("https://api.crafty.gg/api/v2/players/069a79f444e94726a5befca90e38aaf5"));
        const QString hash = "7fd9ba42a7c81eeea22f1524271ae85a8e045ce0af5a6ae16c6406ae917e68b5";
        QCOMPARE(SkinHistory::hashOfUrl("http://textures.minecraft.net/texture/" + hash), hash);
        QCOMPARE(SkinHistory::hashOfUrl(SkinHistory::textureUrl(hash)), hash);
        QCOMPARE(SkinHistory::hashOfUrl("https://example.com/texture/" + hash), QString());
        QCOMPARE(SkinHistory::hashOfUrl(""), QString());
    }

    void test_parse()
    {
        const QString a(64, 'a');
        const QString b(64, 'b');
        auto hidden = skin(QString(64, 'c'), png(64, 64));
        hidden["hidden"] = true;
        auto removed = skin(QString(64, 'd'), png(64, 64));
        removed["deleted_at"] = "2024-01-01T00:00:00.000+00:00";
        const auto entries = SkinHistory::parsePlayer(player({ skin(a, png(64, 64), true), skin(b, png(64, 32)), hidden, removed,
                                                               skin(QString(64, 'e'), png(16, 16)), skin("not a hash", png(64, 64)) }));
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].hash, a);
        QVERIFY(entries[0].slim);
        QVERIFY(!entries[1].slim);
        QCOMPARE(entries[0].wornAt, QDateTime::fromString("2025-01-13T08:20:32.200+01:00", Qt::ISODateWithMs));
        QImage texture;
        QVERIFY(texture.loadFromData(entries[1].png, "PNG"));
        QCOMPARE(texture.size(), QSize(64, 32));
    }

    void test_unknownPlayer()
    {
        QString error;
        QVERIFY(SkinHistory::parsePlayer(R"({"success":false,"message":"Player not found"})", &error).isEmpty());
        QCOMPARE(error, QString("Player not found"));
        error.clear();
        QVERIFY(SkinHistory::parsePlayer("<html>", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(SkinHistory::parsePlayer(QByteArray(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SkinHistoryTest)

#include "SkinHistory_test.moc"
