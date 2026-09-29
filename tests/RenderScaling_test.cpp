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

#include <QTemporaryDir>
#include <QTest>

#include <minecraft/launch/RenderScaling.h>
#include <settings/INISettingsObject.h>

class RenderScalingTest : public QObject {
    Q_OBJECT

   private slots:
    void test_internalSize_data()
    {
        QTest::addColumn<QSize>("output");
        QTest::addColumn<int>("percent");
        QTest::addColumn<QSize>("expected");

        QTest::newRow("half of 1080p") << QSize(1920, 1080) << 50 << QSize(960, 540);
        QTest::newRow("third of the default window") << QSize(854, 480) << 33 << QSize(282, 158);
        QTest::newRow("full size") << QSize(1280, 720) << 100 << QSize(1280, 720);
        QTest::newRow("percent is clamped") << QSize(1280, 720) << 400 << QSize(1280, 720);
        QTest::newRow("never smaller than 64 pixels") << QSize(200, 100) << 10 << QSize(64, 64);
    }
    void test_internalSize()
    {
        QFETCH(QSize, output);
        QFETCH(int, percent);
        QFETCH(QSize, expected);
        QCOMPARE(RenderScaling::internalSize(output, percent), expected);
    }

    void test_arguments_sharp()
    {
        RenderScaling::Config config;
        const auto args = RenderScaling::gamescopeArguments(config, { 1920, 1080 }, { 960, 540 });
        const QStringList expected{
            "-w", "960", "-h", "540", "-W", "1920", "-H", "1080", "-F", "nearest", "-S", "fit", "--force-grab-cursor", "--"
        };
        QCOMPARE(args, expected);
    }

    void test_arguments_smooth_fullscreen()
    {
        RenderScaling::Config config;
        config.filter = "linear";
        config.scaler = "integer";
        config.fullscreen = true;
        config.grabCursor = false;
        const auto args = RenderScaling::gamescopeArguments(config, { 2560, 1440 }, { 640, 360 });
        QVERIFY(args.contains("-f"));
        QVERIFY(!args.contains("--force-grab-cursor"));
        QVERIFY(!args.contains("--sharpness"));
        QCOMPARE(args.at(args.indexOf("-F") + 1), QString("linear"));
        QCOMPARE(args.at(args.indexOf("-S") + 1), QString("integer"));
        QCOMPARE(args.last(), QString("--"));
    }

    void test_arguments_sharpening_and_extra()
    {
        RenderScaling::Config config;
        config.filter = "fsr";
        config.sharpness = 5;
        config.extraArgs = "-r 60 --adaptive-sync";
        const auto args = RenderScaling::gamescopeArguments(config, { 1920, 1080 }, { 1280, 720 });
        QCOMPARE(args.at(args.indexOf("--sharpness") + 1), QString("5"));
        // extra arguments go before the separator, everything after it is the game
        const auto separator = args.indexOf("--");
        QCOMPARE(separator, args.size() - 1);
        QVERIFY(args.indexOf("--adaptive-sync") < separator);
        QCOMPARE(args.at(args.indexOf("-r") + 1), QString("60"));
    }

    void test_readConfig()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        INISettingsObject settings(dir.filePath("test.cfg"));
        settings.registerSetting("RenderScaleEnabled", false);
        settings.registerSetting("RenderScalePercent", 50);
        settings.registerSetting("RenderScaleFilter", "nearest");
        settings.registerSetting("RenderScaleMode", "fit");
        settings.registerSetting("RenderScaleFullscreen", false);
        settings.registerSetting("RenderScaleGrabCursor", true);
        settings.registerSetting("RenderScaleSharpness", 2);
        settings.registerSetting("RenderScaleExtraArgs", "");
        settings.registerSetting("LaunchMaximized", false);
        settings.registerSetting("MinecraftWinWidth", 854);
        settings.registerSetting("MinecraftWinHeight", 480);

        settings.set("RenderScaleEnabled", true);
        settings.set("RenderScalePercent", 3);
        settings.set("RenderScaleFilter", "Blurry");
        settings.set("RenderScaleMode", "STRETCH");
        settings.set("RenderScaleSharpness", 99);

        const auto config = RenderScaling::readConfig(&settings);
        QVERIFY(config.enabled);
        QCOMPARE(config.percent, RenderScaling::s_minPercent);
        // unknown values fall back to the sharp filter instead of passing garbage to gamescope
        QCOMPARE(config.filter, QString("nearest"));
        QCOMPARE(config.scaler, QString("stretch"));
        QCOMPARE(config.sharpness, 20);

        QCOMPARE(RenderScaling::outputSize(&settings, config), QSize(854, 480));
    }
};

QTEST_GUILESS_MAIN(RenderScalingTest)

#include "RenderScaling_test.moc"
