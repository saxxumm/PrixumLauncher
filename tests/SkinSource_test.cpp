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

#include <QTest>

#include <minecraft/skins/SkinSource.h>

using SkinSource::Source;

class SkinSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void test_players()
    {
        QCOMPARE(SkinSource::parse("Notch").kind, Source::Kind::Player);
        QCOMPARE(SkinSource::parse("  jeb_  ").player, QString("jeb_"));
        QCOMPARE(SkinSource::parse("").kind, Source::Kind::None);
        QCOMPARE(SkinSource::parse("two words").kind, Source::Kind::None);
        // too long for a Minecraft name and no URL either
        QCOMPARE(SkinSource::parse("abcdefghijklmnopq").kind, Source::Kind::Url);
    }

    void test_nameMc_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("skin page") << "https://namemc.com/skin/12b92a9206470fe2";
        QTest::newRow("skin page without scheme") << "namemc.com/skin/12b92a9206470fe2";
        QTest::newRow("localized host") << "https://ru.namemc.com/skin/12b92a9206470fe2";
        QTest::newRow("texture") << "https://s.namemc.com/i/12b92a9206470fe2.png";
        QTest::newRow("3d render") << "https://s.namemc.com/3d/skin/body.png?id=12b92a9206470fe2&model=classic&width=150";
    }

    void test_nameMc()
    {
        QFETCH(QString, input);
        const auto source = SkinSource::parse(input);
        QCOMPARE(source.kind, Source::Kind::Url);
        QCOMPARE(source.url, QUrl("https://s.namemc.com/i/12b92a9206470fe2.png"));
        QCOMPARE(source.fileName, QString("namemc-12b92a9206470fe2.png"));
        QVERIFY(source.isSkinLink);
    }

    void test_nameMcProfiles()
    {
        auto source = SkinSource::parse("https://namemc.com/profile/Notch.1");
        QCOMPARE(source.kind, Source::Kind::Player);
        QCOMPARE(source.player, QString("Notch"));
        QCOMPARE(SkinSource::parse("https://namemc.com/profile/Dinnerbone").player, QString("Dinnerbone"));
        // other NameMC pages name nothing that can be downloaded without scraping
        QCOMPARE(SkinSource::parse("https://namemc.com/minecraft-skins/trending").kind, Source::Kind::None);
        QCOMPARE(SkinSource::parse("https://namemc.com/skin/not-a-skin").kind, Source::Kind::None);
    }

    void test_otherLinks()
    {
        auto source =
            SkinSource::parse("https://textures.minecraft.net/texture/292009a4925b58f02c77dadc3ecef07ea4c7472f64e0fdc32ce5522489362680");
        QCOMPARE(source.kind, Source::Kind::Url);
        QCOMPARE(source.fileName, QString("skin-292009a4925b.png"));
        QVERIFY(source.isSkinLink);

        source = SkinSource::parse("https://example.com/skins/knight.png");
        QCOMPARE(source.kind, Source::Kind::Url);
        QCOMPARE(source.fileName, QString("knight.png"));
        QVERIFY(!source.isSkinLink);

        QCOMPARE(SkinSource::parse("https://example.com/download?skin=1").fileName, QString("download.png"));
        QCOMPARE(SkinSource::parse("ftp://example.com/skin.png").kind, Source::Kind::None);
    }
};

QTEST_GUILESS_MAIN(SkinSourceTest)

#include "SkinSource_test.moc"
