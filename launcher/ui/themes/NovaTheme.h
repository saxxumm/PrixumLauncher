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

#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QMap>
#include <QVariant>

#include <memory>

#include "ITheme.h"
#include "Result.h"

/**
 * Nova is the token based design system of the launcher.
 *
 * A theme is a set of named colors and metrics ("tokens"). The base stylesheet (resources/nova/style.qss)
 * references them as @name and is rendered for every theme, so a whole new look only needs a theme.json.
 */
namespace Nova {

struct ColorToken {
    QString key;
    QString label;
    QString group;
};

struct MetricToken {
    QString key;
    QString label;
    int min;
    int max;
    QString suffix;
};

/// the editable base colors, derived colors (accentHover, accentSoft, ...) can be overridden in theme.json too
// built on every call, so the labels follow the current language
QList<ColorToken> colorTokens();
QList<MetricToken> metricTokens();
/// colors which are computed from the base colors when a theme doesn't set them
const QStringList& derivedColorKeys();

struct Tokens {
    QMap<QString, QColor> colors;
    QMap<QString, int> metrics;
    QString fontFamily;

    static Tokens defaults(bool dark = true);

    bool isDark() const;
    QColor color(const QString& key) const;
    int metric(const QString& key) const;

    /// every token as it is substituted into the stylesheet, including derived ones
    QMap<QString, QString> variables() const;
    QPalette palette() const;

    QJsonObject toJson() const;
    /// reads tokens on top of the given base, unknown or invalid values are ignored
    static Tokens fromJson(const QJsonObject& root, Tokens base);

   private:
    QColor derived(const QString& key) const;
};

/// @returns the base stylesheet template shipped with the launcher
QString baseStyleSheet();
/// replaces every known @token in the template
QString renderStyleSheet(const QString& qssTemplate, const QMap<QString, QString>& variables);

/// the tokens of the active theme, or tokens approximated from the palette when a classic theme is used
Tokens current();
bool isActive();
void setCurrent(const Tokens* tokens);

/// the system font before any theme changed it
void setSystemFont(const QFont& font);
QFont systemFont();

}  // namespace Nova

class NovaTheme : public ITheme {
   public:
    NovaTheme(QString id, QString name, Nova::Tokens tokens, QString directory = {});
    ~NovaTheme() override = default;

    static bool isNovaThemeFile(const QString& themeJsonPath);
    static Result<std::unique_ptr<NovaTheme>> fromFile(const QString& themeJsonPath, const QString& id);
    static Result<std::unique_ptr<NovaTheme>> fromJson(const QJsonObject& root, const QString& id, const QString& directory);

    /// writes theme.json (and style.qss when there is custom QSS) into the directory
    static Result<> save(const QString& directory,
                         const QString& name,
                         const Nova::Tokens& tokens,
                         const QString& customQss,
                         bool replaceBaseQss);

    void apply(bool initial) override;
    QString id() override { return m_id; }
    QString name() override { return m_name; }
    QString tooltip() override;
    bool hasStyleSheet() override { return true; }
    QString appStyleSheet() override;
    QString qtTheme() override { return QStringLiteral("Fusion"); }
    QPalette colorScheme() override;
    QColor fadeColor() override;
    double fadeAmount() override { return 0.5; }
    LogColors logColorScheme() override;
    QStringList searchPaths() override;

    const Nova::Tokens& tokens() const { return m_tokens; }
    QString customQss() const { return m_customQss; }
    bool replacesBaseQss() const { return m_replaceBaseQss; }
    /// empty for themes built into the launcher
    QString directory() const { return m_directory; }
    bool isBuiltIn() const { return m_directory.isEmpty(); }
    QStringList watchedFiles() const;
    /// re-read theme.json and style.qss of a theme from disk
    Result<> reload();

    /// render the stylesheet for arbitrary tokens, used by the live preview of the theme editor
    static QString styleSheetFor(const Nova::Tokens& tokens, const QString& customQss, bool replaceBaseQss);
    /// apply tokens to the application without having a theme object, used by the theme editor
    static void applyTokens(const Nova::Tokens& tokens, const QString& customQss, bool replaceBaseQss);

   private:
    QString m_id;
    QString m_name;
    QString m_author;
    Nova::Tokens m_tokens;
    QString m_directory;
    QString m_customQss;
    QString m_customQssFile;
    bool m_replaceBaseQss = false;
};
