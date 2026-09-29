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

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <ui/themes/NovaTheme.h>

class NovaThemeTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() { Q_INIT_RESOURCE(nova); }

    void test_renderStyleSheet()
    {
        const QMap<QString, QString> vars{ { "accent", "#00ff00" }, { "accentSoft", "#3300ff00" }, { "radius", "12px" } };
        const auto result =
            Nova::renderStyleSheet("a { color: @accent; background: @accentSoft; border-radius: @radius; x: @unknown; }", vars);
        QCOMPARE(result, QString("a { color: #00ff00; background: #3300ff00; border-radius: 12px; x: @unknown; }"));
    }

    void test_baseStyleSheetIsComplete()
    {
        // every token used by the shipped stylesheet has to exist, otherwise Qt silently drops the rule
        const auto qss = Nova::baseStyleSheet();
        QVERIFY(!qss.isEmpty());
        static const QRegularExpression s_comments(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption);
        const auto rendered = Nova::renderStyleSheet(QString(qss).remove(s_comments), Nova::Tokens::defaults().variables());
        static const QRegularExpression s_leftover("@[A-Za-z]");
        const auto match = s_leftover.match(rendered);
        QVERIFY2(!match.hasMatch(), qPrintable("unknown token near: " + rendered.mid(match.capturedStart(), 40)));
    }

    void test_darkAndLight()
    {
        QVERIFY(Nova::Tokens::defaults(true).isDark());
        QVERIFY(!Nova::Tokens::defaults(false).isDark());
    }

    void test_derivedColors()
    {
        auto tokens = Nova::Tokens::defaults();
        QVERIFY(tokens.color("accentSoft").alpha() < 255);
        QCOMPARE(tokens.color("accentSoft").rgb(), tokens.color("accent").rgb());

        // explicit values win over calculated ones
        tokens.colors["accentSoft"] = QColor("#123456");
        QCOMPARE(tokens.color("accentSoft"), QColor("#123456"));
        QCOMPARE(tokens.variables().value("accentSoft"), QString("#123456"));
    }

    void test_jsonRoundTrip()
    {
        auto tokens = Nova::Tokens::defaults(true);
        tokens.colors["accent"] = QColor("#ff8800");
        tokens.metrics["radius"] = 3;
        tokens.fontFamily = "Inter";

        // reading on top of light defaults must still give back exactly what was written
        const auto loaded = Nova::Tokens::fromJson(tokens.toJson(), Nova::Tokens::defaults(false));
        QCOMPARE(loaded.colors, tokens.colors);
        QCOMPARE(loaded.metrics, tokens.metrics);
        QCOMPARE(loaded.fontFamily, QString("Inter"));
        QCOMPARE(loaded.variables().value("radius"), QString("3px"));
    }

    void test_saveAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto tokens = Nova::Tokens::defaults(false);
        tokens.colors["accent"] = QColor("#e0529c");
        const QString qss = "QPushButton { border-radius: @radius; }";
        QVERIFY(NovaTheme::save(dir.path(), "Pink", tokens, qss, false));

        QVERIFY(NovaTheme::isNovaThemeFile(dir.filePath("theme.json")));
        auto theme = NovaTheme::fromFile(dir.filePath("theme.json"), "pink");
        QVERIFY(theme);
        QCOMPARE((*theme)->name(), QString("Pink"));
        QCOMPARE((*theme)->tokens().color("accent"), QColor("#e0529c"));
        QVERIFY(!(*theme)->tokens().isDark());
        QCOMPARE((*theme)->customQss(), qss);
        QVERIFY((*theme)->appStyleSheet().contains("QPushButton { border-radius: 12px; }"));
    }

    void test_presetsLoad()
    {
        const QDir presets(":/nova/presets");
        const auto files = presets.entryList({ "*.json" });
        QVERIFY(files.size() >= 2);
        for (const auto& file : files) {
            auto theme = NovaTheme::fromFile(presets.filePath(file), file);
            QVERIFY2(theme, qPrintable(file));
            for (const auto& token : Nova::colorTokens()) {
                QVERIFY2((*theme)->tokens().colors.contains(token.key), qPrintable(file + ": " + token.key));
            }
        }
    }
};

QTEST_MAIN(NovaThemeTest)

#include "NovaTheme_test.moc"
