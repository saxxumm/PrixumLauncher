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

#include "NovaTheme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMenu>
#include <QRegularExpression>
#include <QStyleFactory>

#include <algorithm>
#include <cmath>
#include <optional>

#include "FileSystem.h"
#include "HintOverrideProxyStyle.h"
#include "Json.h"
#include "ThemeManager.h"

namespace Nova {

namespace {

std::optional<Tokens> s_current;
std::optional<QFont> s_systemFont;

QColor mix(const QColor& a, const QColor& b, double t)
{
    auto lerp = [t](int x, int y) { return static_cast<int>(std::lround(x + (y - x) * t)); };
    return QColor(lerp(a.red(), b.red()), lerp(a.green(), b.green()), lerp(a.blue(), b.blue()), lerp(a.alpha(), b.alpha()));
}

QColor withAlpha(QColor color, double alpha)
{
    color.setAlphaF(static_cast<float>(alpha));
    return color;
}

QString css(const QColor& color)
{
    return color.alpha() == 255 ? color.name(QColor::HexRgb) : color.name(QColor::HexArgb);
}

QString px(int value)
{
    return QString::number(value) + "px";
}

QString pt(double value)
{
    return QString::number(value, 'f', 1) + "pt";
}

}  // namespace

const QList<ColorToken>& colorTokens()
{
    static const QList<ColorToken> tokens{
        { "window", QObject::tr("Window background"), QObject::tr("Surfaces") },
        { "sidebar", QObject::tr("Sidebar"), QObject::tr("Surfaces") },
        { "surface", QObject::tr("Panels and cards"), QObject::tr("Surfaces") },
        { "surfaceAlt", QObject::tr("Inputs and lists"), QObject::tr("Surfaces") },
        { "hover", QObject::tr("Hover highlight"), QObject::tr("Surfaces") },
        { "border", QObject::tr("Borders"), QObject::tr("Surfaces") },
        { "text", QObject::tr("Text"), QObject::tr("Text") },
        { "textMuted", QObject::tr("Secondary text"), QObject::tr("Text") },
        { "link", QObject::tr("Links"), QObject::tr("Text") },
        { "accent", QObject::tr("Accent"), QObject::tr("Accent") },
        { "accentText", QObject::tr("Text on accent"), QObject::tr("Accent") },
        { "success", QObject::tr("Success / running"), QObject::tr("Status") },
        { "warning", QObject::tr("Warning"), QObject::tr("Status") },
        { "danger", QObject::tr("Danger / errors"), QObject::tr("Status") },
        { "tooltip", QObject::tr("Tooltip background"), QObject::tr("Status") },
        { "tooltipText", QObject::tr("Tooltip text"), QObject::tr("Status") },
    };
    return tokens;
}

const QList<MetricToken>& metricTokens()
{
    static const QList<MetricToken> tokens{
        { "radius", QObject::tr("Corner radius"), 0, 24, "px" },
        { "controlRadius", QObject::tr("Button and input corner radius"), 0, 20, "px" },
        { "density", QObject::tr("Control padding"), 2, 14, "px" },
        { "fontSize", QObject::tr("Font size (0 = system)"), 0, 24, "pt" },
        { "sidebarWidth", QObject::tr("Sidebar width"), 170, 360, "px" },
        { "inspectorWidth", QObject::tr("Instance panel width"), 230, 440, "px" },
        { "cardWidth", QObject::tr("Instance tile width"), 72, 240, "px" },
        { "iconSize", QObject::tr("Instance icon size"), 24, 128, "px" },
    };
    return tokens;
}

const QStringList& derivedColorKeys()
{
    static const QStringList keys{ "accentHover",  "accentPressed", "accentSoft",   "pressed", "borderStrong",
                                   "textDisabled", "surfaceAlt2",   "scrollHandle", "knob",    "scrollHandleHover",
                                   "successSoft",  "warningSoft",   "dangerSoft" };
    return keys;
}

Tokens Tokens::defaults(bool dark)
{
    Tokens t;
    if (dark) {
        t.colors = {
            { "window", QColor("#101216") },      { "sidebar", QColor("#0c0e11") },    { "surface", QColor("#171a20") },
            { "surfaceAlt", QColor("#1d2129") },  { "hover", QColor("#262b35") },      { "border", QColor("#2a2f3a") },
            { "text", QColor("#e7e9ee") },        { "textMuted", QColor("#8e96a8") },  { "link", QColor("#6fc3ff") },
            { "accent", QColor("#3ecf8e") },      { "accentText", QColor("#05140d") }, { "success", QColor("#3ecf8e") },
            { "warning", QColor("#f5b83d") },     { "danger", QColor("#f2555a") },     { "tooltip", QColor("#232833") },
            { "tooltipText", QColor("#e7e9ee") },
        };
    } else {
        t.colors = {
            { "window", QColor("#f3f4f7") },      { "sidebar", QColor("#eceef2") },    { "surface", QColor("#ffffff") },
            { "surfaceAlt", QColor("#f7f8fa") },  { "hover", QColor("#eaecf1") },      { "border", QColor("#dde0e7") },
            { "text", QColor("#1b1f27") },        { "textMuted", QColor("#636b7c") },  { "link", QColor("#1a73e8") },
            { "accent", QColor("#16a36a") },      { "accentText", QColor("#ffffff") }, { "success", QColor("#16a36a") },
            { "warning", QColor("#c98a0b") },     { "danger", QColor("#d93d42") },     { "tooltip", QColor("#1b1f27") },
            { "tooltipText", QColor("#f3f4f7") },
        };
    }
    t.metrics = {
        { "radius", 12 },        { "controlRadius", 8 },    { "density", 6 },     { "fontSize", 0 },
        { "sidebarWidth", 240 }, { "inspectorWidth", 300 }, { "cardWidth", 112 }, { "iconSize", 48 },
    };
    return t;
}

bool Tokens::isDark() const
{
    return colors.value("window", QColor("#101216")).lightnessF() < 0.5;
}

QColor Tokens::color(const QString& key) const
{
    if (auto it = colors.find(key); it != colors.end() && it->isValid()) {
        return *it;
    }
    if (derivedColorKeys().contains(key)) {
        return derived(key);
    }
    return defaults(isDark()).colors.value(key);
}

QColor Tokens::derived(const QString& key) const
{
    const bool dark = isDark();
    const QColor accent = color("accent");
    const QColor text = color("text");
    if (key == "accentHover")
        return mix(accent, dark ? QColor(Qt::white) : QColor(Qt::black), 0.12);
    if (key == "accentPressed")
        return mix(accent, Qt::black, 0.2);
    if (key == "accentSoft")
        return withAlpha(accent, dark ? 0.2 : 0.14);
    if (key == "pressed")
        return mix(color("hover"), text, 0.08);
    if (key == "borderStrong")
        return mix(color("border"), text, 0.22);
    if (key == "textDisabled")
        return mix(text, color("window"), 0.58);
    if (key == "surfaceAlt2")
        return mix(color("surfaceAlt"), text, 0.03);
    if (key == "scrollHandle")
        return mix(color("surfaceAlt"), text, 0.22);
    if (key == "scrollHandleHover")
        return mix(color("surfaceAlt"), text, 0.38);
    if (key == "knob")
        return QColor(Qt::white);
    if (key == "successSoft")
        return withAlpha(color("success"), 0.15);
    if (key == "warningSoft")
        return withAlpha(color("warning"), 0.15);
    if (key == "dangerSoft")
        return withAlpha(color("danger"), 0.15);
    return {};
}

int Tokens::metric(const QString& key) const
{
    if (auto it = metrics.find(key); it != metrics.end()) {
        return *it;
    }
    return defaults().metrics.value(key);
}

QMap<QString, QString> Tokens::variables() const
{
    QMap<QString, QString> vars;
    for (const auto& token : colorTokens()) {
        vars[token.key] = css(color(token.key));
    }
    for (const auto& key : derivedColorKeys()) {
        vars[key] = css(color(key));
    }
    // anything else a theme defines can be used in custom QSS as well
    for (auto it = colors.cbegin(); it != colors.cend(); ++it) {
        if (!vars.contains(it.key()) && it->isValid()) {
            vars[it.key()] = css(*it);
        }
    }

    for (const auto& token : metricTokens()) {
        vars[token.key] = token.suffix == "pt" ? pt(metric(token.key)) : px(metric(token.key));
    }
    vars["radiusSmall"] = px(std::max(2, metric("controlRadius") - 3));
    vars["padY"] = px(metric("density"));
    vars["padX"] = px(metric("density") * 2 + 2);

    double base = metric("fontSize");
    if (base <= 0) {
        base = systemFont().pointSizeF();
    }
    if (base <= 0) {
        base = 10;
    }
    vars["fontSize"] = pt(base);
    vars["titleSize"] = pt(base * 1.5);
    vars["subtitleSize"] = pt(base * 1.2);
    vars["smallSize"] = pt(base * 0.85);
    return vars;
}

QPalette Tokens::palette() const
{
    QPalette p;
    auto set = [&p](QPalette::ColorRole role, const QColor& value) { p.setColor(role, value); };
    set(QPalette::Window, color("window"));
    set(QPalette::WindowText, color("text"));
    set(QPalette::Base, color("surfaceAlt"));
    set(QPalette::AlternateBase, color("surfaceAlt2"));
    set(QPalette::ToolTipBase, color("tooltip"));
    set(QPalette::ToolTipText, color("tooltipText"));
    set(QPalette::PlaceholderText, color("textMuted"));
    set(QPalette::Text, color("text"));
    set(QPalette::Button, color("surfaceAlt"));
    set(QPalette::ButtonText, color("text"));
    set(QPalette::BrightText, Qt::white);
    set(QPalette::Light, mix(color("hover"), color("text"), 0.1));
    set(QPalette::Midlight, color("hover"));
    set(QPalette::Mid, color("border"));
    set(QPalette::Dark, color("borderStrong"));
    set(QPalette::Shadow, QColor(0, 0, 0, 160));
    set(QPalette::Highlight, color("accent"));
    set(QPalette::HighlightedText, color("accentText"));
    set(QPalette::Link, color("link"));
    set(QPalette::LinkVisited, color("link").darker(115));
    set(QPalette::Accent, color("accent"));

    const QColor disabledText = color("textDisabled");
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText }) {
        p.setColor(QPalette::Disabled, role, disabledText);
    }
    p.setColor(QPalette::Disabled, QPalette::Base, color("surface"));
    p.setColor(QPalette::Disabled, QPalette::Button, color("surface"));
    p.setColor(QPalette::Disabled, QPalette::Highlight, mix(color("accent"), color("window"), 0.5));
    return p;
}

QJsonObject Tokens::toJson() const
{
    QJsonObject colorsObject;
    for (auto it = colors.cbegin(); it != colors.cend(); ++it) {
        if (it->isValid()) {
            colorsObject[it.key()] = css(*it);
        }
    }
    QJsonObject metricsObject;
    for (auto it = metrics.cbegin(); it != metrics.cend(); ++it) {
        metricsObject[it.key()] = *it;
    }
    metricsObject["fontFamily"] = fontFamily;

    QJsonObject root;
    root["colors"] = colorsObject;
    root["metrics"] = metricsObject;
    return root;
}

Tokens Tokens::fromJson(const QJsonObject& root, Tokens base)
{
    const auto colorsObject = root["colors"].toObject();
    for (auto it = colorsObject.constBegin(); it != colorsObject.constEnd(); ++it) {
        const QColor value = QColor::fromString(it.value().toString());
        if (value.isValid()) {
            base.colors[it.key()] = value;
        } else {
            themeWarningLog() << "Ignoring invalid color" << it.value().toString() << "for" << it.key();
        }
    }
    const auto metricsObject = root["metrics"].toObject();
    for (auto it = metricsObject.constBegin(); it != metricsObject.constEnd(); ++it) {
        if (it.key() == "fontFamily") {
            base.fontFamily = it.value().toString();
        } else if (it.value().isDouble()) {
            base.metrics[it.key()] = it.value().toInt();
        }
    }
    return base;
}

QString baseStyleSheet()
{
    QFile file(":/nova/style.qss");
    if (!file.open(QIODevice::ReadOnly)) {
        themeWarningLog() << "Couldn't open the Nova base stylesheet";
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString renderStyleSheet(const QString& qssTemplate, const QMap<QString, QString>& variables)
{
    static const QRegularExpression s_token("@([A-Za-z][A-Za-z0-9_]*)");
    QString result;
    result.reserve(qssTemplate.size() + qssTemplate.size() / 4);
    qsizetype last = 0;
    auto it = s_token.globalMatch(qssTemplate);
    while (it.hasNext()) {
        const auto match = it.next();
        const auto name = match.captured(1);
        result += QStringView(qssTemplate).mid(last, match.capturedStart() - last);
        if (auto value = variables.find(name); value != variables.end()) {
            result += *value;
        } else {
            result += match.captured(0);
        }
        last = match.capturedEnd();
    }
    result += QStringView(qssTemplate).mid(last);
    return result;
}

bool isActive()
{
    return s_current.has_value();
}

void setCurrent(const Tokens* tokens)
{
    if (tokens) {
        s_current = *tokens;
    } else {
        s_current.reset();
    }
}

Tokens current()
{
    if (s_current) {
        return *s_current;
    }
    // approximate the tokens from whatever classic theme is active
    const QPalette palette = QApplication::palette();
    Tokens t = Tokens::defaults(palette.color(QPalette::Window).lightnessF() < 0.5);
    t.colors["window"] = palette.color(QPalette::Window);
    t.colors["sidebar"] = palette.color(QPalette::Window);
    t.colors["surface"] = palette.color(QPalette::Base);
    t.colors["surfaceAlt"] = palette.color(QPalette::Base);
    t.colors["text"] = palette.color(QPalette::WindowText);
    t.colors["textMuted"] = palette.color(QPalette::PlaceholderText);
    t.colors["border"] = palette.color(QPalette::Mid);
    t.colors["hover"] = palette.color(QPalette::Midlight);
    t.colors["accent"] = palette.color(QPalette::Highlight);
    t.colors["accentText"] = palette.color(QPalette::HighlightedText);
    t.colors["link"] = palette.color(QPalette::Link);
    return t;
}

void setSystemFont(const QFont& font)
{
    s_systemFont = font;
}

QFont systemFont()
{
    return s_systemFont.value_or(QApplication::font());
}

}  // namespace Nova

namespace {

class NovaProxyStyle : public HintOverrideProxyStyle {
   public:
    explicit NovaProxyStyle(QStyle* style) : HintOverrideProxyStyle(style) { setProperty("novaStyle", true); }

    void polish(QWidget* widget) override
    {
        // rounded menus need a transparent window behind them
        if (qobject_cast<QMenu*>(widget)) {
            widget->setAttribute(Qt::WA_TranslucentBackground);
        }
        HintOverrideProxyStyle::polish(widget);
    }
    using HintOverrideProxyStyle::polish;

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget, QStyleHintReturn* returnData) const override
    {
        // plain text buttons, the old colorful OK/Cancel icons don't fit the flat look
        if (hint == SH_DialogButtonBox_ButtonsHaveIcons) {
            return 0;
        }
        return HintOverrideProxyStyle::styleHint(hint, option, widget, returnData);
    }

    int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override
    {
        switch (metric) {
            case PM_ToolBarIconSize:
                return 18;
            case PM_MenuHMargin:
            case PM_MenuVMargin:
                return 0;
            default:
                return HintOverrideProxyStyle::pixelMetric(metric, option, widget);
        }
    }
};

// images used by the stylesheet, written to disk with the theme colors baked in
const QList<std::pair<QString, QString>> s_assetShapes{
    { "check",
      R"(<path d="M5.5 12.5l4.2 4.2L18.5 7.8" fill="none" stroke="%1" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>)" },
    { "partial", R"(<path d="M6.5 12h11" fill="none" stroke="%1" stroke-width="3" stroke-linecap="round"/>)" },
    { "radio", R"(<circle cx="12" cy="12" r="5" fill="%1"/>)" },
    { "chevron-down",
      R"(<path d="M6 9.5l6 6 6-6" fill="none" stroke="%1" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/>)" },
    { "chevron-up",
      R"(<path d="M6 14.5l6-6 6 6" fill="none" stroke="%1" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/>)" },
    { "chevron-right",
      R"(<path d="M9.5 6l6 6-6 6" fill="none" stroke="%1" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/>)" },
    { "chevron-left",
      R"(<path d="M14.5 6l-6 6 6 6" fill="none" stroke="%1" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/>)" },
};
const QStringList s_assetColors{ "text", "textMuted", "textDisabled", "accent", "accentText" };

// with an application stylesheet Qt wraps our style, it then lives on as a child of the wrapper
bool novaStyleActive()
{
    auto* style = QApplication::style();
    if (style->property("novaStyle").toBool()) {
        return true;
    }
    const auto children = style->findChildren<QStyle*>();
    return std::any_of(children.begin(), children.end(), [](QStyle* child) { return child->property("novaStyle").toBool(); });
}

void prepareAssets(const Nova::Tokens& tokens)
{
    QStringList colorNames;
    for (const auto& key : s_assetColors) {
        colorNames << tokens.color(key).name(QColor::HexArgb);
    }
    // the shape list is part of the hash, so new assets are written after updates
    QStringList shapes;
    for (const auto& [name, shape] : s_assetShapes) {
        shapes << name;
    }
    const auto hash = QString::number(qHash(colorNames.join(',') + shapes.join(',')), 16);
    const QDir dir(QDir("cache/nova").absoluteFilePath(hash));
    const QString marker = dir.absoluteFilePath(".complete");

    if (!QFileInfo::exists(marker)) {
        if (!FS::ensureFolderPathExists(dir.absolutePath())) {
            themeWarningLog() << "Couldn't create the Nova asset folder" << dir.absolutePath();
        }
        for (qsizetype i = 0; i < s_assetColors.size(); i++) {
            for (const auto& [name, shape] : s_assetShapes) {
                const QString svg =
                    QString(R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 24 24">%1</svg>)")
                        .arg(shape.arg(tokens.color(s_assetColors[i]).name(QColor::HexRgb)));
                if (auto res = FS::write(dir.absoluteFilePath(name + "-" + s_assetColors[i] + ".svg"), svg.toUtf8()); !res) {
                    themeWarningLog() << "Couldn't write Nova asset:" << res.error();
                }
            }
        }
        if (auto res = FS::write(marker, QByteArray()); !res) {
            themeWarningLog() << "Couldn't finish Nova assets:" << res.error();
        }
    }
    QDir::setSearchPaths("nova", { dir.absolutePath() });
}

void applyFont(const Nova::Tokens& tokens)
{
    QFont font = Nova::systemFont();
    if (!tokens.fontFamily.isEmpty()) {
        font.setFamilies({ tokens.fontFamily });
    }
    if (tokens.metric("fontSize") > 0) {
        font.setPointSize(tokens.metric("fontSize"));
    }
    QApplication::setFont(font);
}

}  // namespace

NovaTheme::NovaTheme(QString id, QString name, Nova::Tokens tokens, QString directory)
    : m_id(std::move(id)), m_name(std::move(name)), m_tokens(std::move(tokens)), m_directory(std::move(directory))
{}

bool NovaTheme::isNovaThemeFile(const QString& themeJsonPath)
{
    auto root = Json::requireObject(themeJsonPath, "Theme JSON file");
    return root && (*root)["format"].toString() == "nova";
}

Result<std::unique_ptr<NovaTheme>> NovaTheme::fromFile(const QString& themeJsonPath, const QString& id)
{
    TRY_INTO(const auto root, Json::requireObject(themeJsonPath, "Theme JSON file"))
    return fromJson(root, id, QFileInfo(themeJsonPath).absolutePath());
}

Result<std::unique_ptr<NovaTheme>> NovaTheme::fromJson(const QJsonObject& root, const QString& id, const QString& directory)
{
    const bool dark = root["base"].toString("dark") != "light";
    auto tokens = Nova::Tokens::fromJson(root, Nova::Tokens::defaults(dark));
    auto theme = std::make_unique<NovaTheme>(id, root["name"].toString(id), tokens, directory);
    theme->m_author = root["author"].toString();
    theme->m_replaceBaseQss = root["replaceBaseQss"].toBool(false);

    if (!directory.isEmpty()) {
        const QString qssName = root["customQss"].toString("style.qss");
        const QString qssPath = QDir(directory).absoluteFilePath(qssName);
        if (QFileInfo(qssPath).isFile()) {
            auto qss = FS::read(qssPath);
            if (!qss) {
                themeWarningLog() << "Couldn't read" << qssPath << ":" << qss.error();
            } else {
                theme->m_customQss = QString::fromUtf8(*qss);
                theme->m_customQssFile = qssPath;
            }
        }
    }
    return theme;
}

Result<> NovaTheme::save(const QString& directory,
                         const QString& name,
                         const Nova::Tokens& tokens,
                         const QString& customQss,
                         bool replaceBaseQss)
{
    if (!FS::ensureFolderPathExists(directory)) {
        return std::unexpected(QObject::tr("Couldn't create the folder %1").arg(directory));
    }
    QJsonObject root = tokens.toJson();
    root["format"] = "nova";
    root["name"] = name;
    root["base"] = tokens.isDark() ? "dark" : "light";
    root["replaceBaseQss"] = replaceBaseQss;
    root["customQss"] = "style.qss";

    const QDir dir(directory);
    TRY(FS::write(dir.absoluteFilePath("theme.json"), QJsonDocument(root).toJson(QJsonDocument::Indented)))

    const QString qssPath = dir.absoluteFilePath("style.qss");
    if (!customQss.trimmed().isEmpty()) {
        TRY(FS::write(qssPath, customQss.toUtf8()))
    } else if (QFileInfo::exists(qssPath)) {
        QFile::remove(qssPath);
    }
    return {};
}

QString NovaTheme::tooltip()
{
    if (!m_author.isEmpty()) {
        return QObject::tr("Nova theme by %1").arg(m_author);
    }
    return isBuiltIn() ? QObject::tr("Built-in Nova theme") : QObject::tr("Custom Nova theme from %1").arg(m_directory);
}

QString NovaTheme::styleSheetFor(const Nova::Tokens& tokens, const QString& customQss, bool replaceBaseQss)
{
    const auto vars = tokens.variables();
    QString qss = replaceBaseQss ? QString() : Nova::renderStyleSheet(Nova::baseStyleSheet(), vars);
    if (!customQss.trimmed().isEmpty()) {
        qss += "\n/* ---------- theme stylesheet ---------- */\n" + Nova::renderStyleSheet(customQss, vars);
    }
    return qss;
}

QString NovaTheme::appStyleSheet()
{
    return styleSheetFor(m_tokens, m_customQss, m_replaceBaseQss);
}

QPalette NovaTheme::colorScheme()
{
    return m_tokens.palette();
}

QColor NovaTheme::fadeColor()
{
    return m_tokens.color("window");
}

LogColors NovaTheme::logColorScheme()
{
    LogColors colors = defaultLogColors(colorScheme());
    colors.foreground[MessageLevel::Launcher] = m_tokens.color("link");
    colors.foreground[MessageLevel::Debug] = m_tokens.color("textMuted");
    colors.foreground[MessageLevel::Warning] = m_tokens.color("warning");
    colors.foreground[MessageLevel::Error] = m_tokens.color("danger");
    colors.foreground[MessageLevel::Fatal] = m_tokens.color("danger");
    colors.background[MessageLevel::Fatal] = m_tokens.color("dangerSoft");
    return colors;
}

QStringList NovaTheme::searchPaths()
{
    if (m_directory.isEmpty()) {
        return {};
    }
    const QString resources = QDir(m_directory).absoluteFilePath("resources");
    return QFileInfo::exists(resources) ? QStringList{ resources, m_directory } : QStringList{ m_directory };
}

QStringList NovaTheme::watchedFiles() const
{
    if (m_directory.isEmpty()) {
        return {};
    }
    QStringList files{ QDir(m_directory).absoluteFilePath("theme.json") };
    files << (m_customQssFile.isEmpty() ? QDir(m_directory).absoluteFilePath("style.qss") : m_customQssFile);
    return files;
}

Result<> NovaTheme::reload()
{
    if (m_directory.isEmpty()) {
        return {};
    }
    auto loaded = fromFile(QDir(m_directory).absoluteFilePath("theme.json"), m_id);
    if (!loaded) {
        return std::unexpected(loaded.error());
    }
    const auto& fresh = *loaded;
    m_name = fresh->m_name;
    m_author = fresh->m_author;
    m_tokens = fresh->m_tokens;
    m_customQss = fresh->m_customQss;
    m_customQssFile = fresh->m_customQssFile;
    m_replaceBaseQss = fresh->m_replaceBaseQss;
    return {};
}

void NovaTheme::applyTokens(const Nova::Tokens& tokens, const QString& customQss, bool replaceBaseQss)
{
    Nova::setCurrent(&tokens);
    prepareAssets(tokens);
    applyFont(tokens);
    if (!novaStyleActive()) {
        qApp->setStyleSheet(QString());
        QApplication::setStyle(new NovaProxyStyle(QStyleFactory::create("Fusion")));
    }
    QApplication::setPalette(tokens.palette());
    qApp->setStyleSheet(styleSheetFor(tokens, customQss, replaceBaseQss));
}

void NovaTheme::apply(bool)
{
    Nova::setCurrent(&m_tokens);
    prepareAssets(m_tokens);
    applyFont(m_tokens);
    qApp->setStyleSheet(QString());
    QApplication::setStyle(new NovaProxyStyle(QStyleFactory::create(qtTheme())));
    QApplication::setPalette(colorScheme());
    qApp->setStyleSheet(appStyleSheet());
    QDir::setSearchPaths("theme", searchPaths());
}
