// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 Tayou <git@tayou.org>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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
#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QDirListing>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QImageReader>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include "Result.h"
#include "ui/themes/BrightTheme.h"
#include "ui/themes/CatPack.h"
#include "ui/themes/CustomTheme.h"
#include "ui/themes/DarkTheme.h"
#include "ui/themes/NovaIcons.h"
#include "ui/themes/NovaTheme.h"
#include "ui/themes/SystemTheme.h"

#include "Application.h"
#include "settings/SettingsObject.h"

ThemeManager::ThemeManager()
{
    QIcon::setFallbackThemeName(QIcon::themeName());
    QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << m_iconThemeFolder.path());

    themeDebugLog() << "Determining System Widget Theme...";
    const auto& style = QApplication::style();
    m_defaultStyle = style->objectName();
    themeDebugLog() << "System theme seems to be:" << m_defaultStyle;

    m_defaultPalette = QApplication::palette();
    Nova::setSystemFont(QApplication::font());

    m_themeWatcher = std::make_unique<QFileSystemWatcher>();
    auto scheduleReload = [this] {
        // editors often save in several steps, only reload once they are done
        if (!m_reloadPending) {
            m_reloadPending = true;
            QTimer::singleShot(250, m_themeWatcher.get(), [this] { reloadCurrentTheme(); });
        }
    };
    QObject::connect(m_themeWatcher.get(), &QFileSystemWatcher::fileChanged, m_themeWatcher.get(), scheduleReload);
    QObject::connect(m_themeWatcher.get(), &QFileSystemWatcher::directoryChanged, m_themeWatcher.get(), scheduleReload);

    initializeThemes();
    initializeCatPacks();
}

ThemeManager::~ThemeManager()
{
    stopSettingNewWindowColorsOnMac();
}

/// @brief Adds the Theme to the list of themes
/// @param theme The Theme to add
/// @return Theme ID
QString ThemeManager::addTheme(std::unique_ptr<ITheme> theme)
{
    QString id = theme->id();
    if (!m_themes.contains(id)) {
        m_themes.emplace(id, std::move(theme));
    } else {
        themeWarningLog() << "Theme(" << id << ") not added to prevent id duplication";
    }
    return id;
}

/// @brief Gets the Theme from the List via ID
/// @param themeId Theme ID of theme to fetch
/// @return Theme at themeId
ITheme* ThemeManager::getTheme(const QString& themeId)
{
    return m_themes[themeId].get();
}

QString ThemeManager::addIconTheme(IconTheme theme)
{
    QString id = theme.id();
    if (!m_icons.contains(id)) {
        m_icons.emplace(id, std::move(theme));
    } else {
        themeWarningLog() << "IconTheme(" << id << ") not added to prevent id duplication";
    }
    return id;
}

void ThemeManager::initializeThemes()
{
    // Icon themes
    initializeIcons();

    // Initialize widget themes
    initializeWidgets();
}

void ThemeManager::initializeIcons()
{
    // TODO: icon themes and instance icons do not mesh well together. Rearrange and fix discrepancies!
    // set icon theme search path!
    themeDebugLog() << "<> Initializing Icon Themes";

    for (const QString& id : s_builtinIcons) {
        IconTheme theme(id, QString(":/icons/%1").arg(id));
        if (!theme.load()) {
            themeWarningLog() << "Couldn't load built-in icon theme" << id;
            continue;
        }

        addIconTheme(std::move(theme));
        themeDebugLog() << "Loaded Built-In Icon Theme" << id;
    }

    // monochrome icons which follow the colors of the application theme
    if (IconTheme nova(NovaIcons::s_iconThemeId, ":/nova/icontheme"); nova.load()) {
        addIconTheme(std::move(nova));
        themeDebugLog() << "Loaded Built-In Icon Theme" << NovaIcons::s_iconThemeId;
    }

    if (!m_iconThemeFolder.mkpath(".")) {
        themeWarningLog() << "Couldn't create icon theme folder";
    }
    themeDebugLog() << "Icon Theme Folder Path:" << m_iconThemeFolder.absolutePath();

    for (const auto& entry :
         QDirListing(m_iconThemeFolder.path(), QDirListing::IteratorFlag::DirsOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
        QDir dir(entry.filePath());
        IconTheme theme(dir.dirName(), dir.path());
        if (!theme.load()) {
            continue;
        }

        addIconTheme(std::move(theme));
        themeDebugLog() << "Loaded Custom Icon Theme from" << dir.path();
    }

    themeDebugLog() << "<> Icon themes initialized.";
}

void ThemeManager::initializeWidgets()
{
    themeDebugLog() << "<> Initializing Widget Themes";
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<SystemTheme>(m_defaultStyle, m_defaultPalette, true));
    auto darkThemeId = addTheme(std::make_unique<DarkTheme>());
    themeDebugLog() << "Loading Built-in Theme:" << darkThemeId;
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<BrightTheme>());

    themeDebugLog() << "<> Initializing Nova Themes";
    for (const auto& preset : QDir(":/nova/presets").entryInfoList({ "*.json" }, QDir::Files, QDir::Name)) {
        auto theme = NovaTheme::fromFile(preset.filePath(), preset.completeBaseName());
        if (!theme) {
            themeWarningLog() << "Couldn't load built-in Nova theme" << preset.fileName() << ":" << theme.error();
            continue;
        }
        // built-in presets don't live in a folder
        auto builtIn = std::make_unique<NovaTheme>((*theme)->id(), (*theme)->name(), (*theme)->tokens());
        themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::move(builtIn));
    }

    themeDebugLog() << "<> Initializing System Widget Themes";
    QStringList styles = QStyleFactory::keys();
    for (auto& st : styles) {
#ifdef Q_OS_WINDOWS
        if (QSysInfo::productVersion() != "11" && st == "windows11") {
            continue;
        }
#endif
        themeDebugLog() << "Loading System Theme:" << addTheme(std::make_unique<SystemTheme>(st, m_defaultPalette, false));
    }

    // TODO: need some way to differentiate same name themes in different subdirectories
    //  (maybe smaller grey text next to theme name in dropdown?)

    if (!m_applicationThemeFolder.mkpath(".")) {
        themeWarningLog() << "Couldn't create theme folder";
    }
    themeDebugLog() << "Theme Folder Path:" << m_applicationThemeFolder.absolutePath();

    for (const auto& directoryEntry :
         QDirListing(m_applicationThemeFolder.path(), QDirListing::IteratorFlag::DirsOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
        QDir dir(directoryEntry.filePath());
        QFileInfo themeJson(dir.absoluteFilePath("theme.json"));
        if (themeJson.exists() && NovaTheme::isNovaThemeFile(themeJson.absoluteFilePath())) {
            // Load Nova token themes
            themeDebugLog() << "Loading Nova Theme from:" << themeJson.absoluteFilePath();
            auto theme = NovaTheme::fromFile(themeJson.absoluteFilePath(), dir.dirName());
            if (theme) {
                addTheme(std::move(*theme));
            } else {
                themeWarningLog() << "Couldn't load Nova theme:" << theme.error();
            }
        } else if (themeJson.exists()) {
            // Load "theme.json" based themes
            themeDebugLog() << "Loading JSON Theme from:" << themeJson.absoluteFilePath();
            addTheme(std::make_unique<CustomTheme>(getTheme(darkThemeId), themeJson, true));
        } else {
            // Load pure QSS Themes
            for (const auto& stylesheetEntry :
                 QDirListing(dir.absoluteFilePath(""), { "*.qss", "*.css" },
                             QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
                QFile customThemeFile(stylesheetEntry.absoluteFilePath());
                QFileInfo customThemeFileInfo(customThemeFile);
                themeDebugLog() << "Loading QSS Theme from:" << customThemeFileInfo.absoluteFilePath();
                addTheme(std::make_unique<CustomTheme>(getTheme(darkThemeId), customThemeFileInfo, false));
            }
        }
    }

    themeDebugLog() << "<> Widget themes initialized.";
}

#ifndef Q_OS_MACOS
void ThemeManager::setTitlebarColorOnMac(WId windowId, const QColor& color) {}
void ThemeManager::setTitlebarColorOfAllWindowsOnMac(const QColor& color) {}
void ThemeManager::stopSettingNewWindowColorsOnMac() {}
#endif

QList<IconTheme*> ThemeManager::getValidIconThemes()
{
    QList<IconTheme*> ret;
    ret.reserve(static_cast<qsizetype>(m_icons.size()));
    for (auto&& [id, theme] : m_icons) {
        ret.append(&theme);
    }
    return ret;
}

QList<ITheme*> ThemeManager::getValidApplicationThemes()
{
    QList<ITheme*> ret;
    ret.reserve(static_cast<qsizetype>(m_themes.size()));
    for (auto&& [id, theme] : m_themes) {
        ret.append(theme.get());
    }
    return ret;
}

QList<CatPack*> ThemeManager::getValidCatPacks()
{
    QList<CatPack*> ret;
    ret.reserve(static_cast<qsizetype>(m_catPacks.size()));
    for (auto&& [id, theme] : m_catPacks) {
        ret.append(theme.get());
    }
    return ret;
}

bool ThemeManager::isValidIconTheme(const QString& id)
{
    return !id.isEmpty() && m_icons.contains(id);
}

bool ThemeManager::isValidApplicationTheme(const QString& id)
{
    return !id.isEmpty() && m_themes.contains(id);
}

QDir ThemeManager::getIconThemesFolder()
{
    return m_iconThemeFolder;
}

QDir ThemeManager::getApplicationThemesFolder()
{
    return m_applicationThemeFolder;
}

QDir ThemeManager::getCatPacksFolder()
{
    return m_catPacksFolder;
}

void ThemeManager::setIconTheme(const QString& name)
{
    if (!m_icons.contains(name)) {
        themeWarningLog() << "Tried to set invalid icon theme:" << name;
        return;
    }

    m_currentIconThemeId = name;
    if (name == NovaIcons::s_iconThemeId) {
        QIcon::setThemeName(NovaIcons::prepareIconTheme());
        return;
    }
    QIcon::setThemeName(name);
}

void ThemeManager::refreshIconTheme()
{
    if (m_currentIconThemeId == NovaIcons::s_iconThemeId) {
        QIcon::setThemeName(NovaIcons::prepareIconTheme());
    }
}

void ThemeManager::setApplicationTheme(const QString& name, bool initial)
{
    auto systemPalette = qApp->palette();
    auto themeIter = m_themes.find(name);
    if (themeIter != m_themes.end()) {
        auto& theme = themeIter->second;
        themeDebugLog() << "applying theme" << theme->name();
        // undo what a previous Nova theme might have changed
        Nova::setCurrent(nullptr);
        QApplication::setFont(Nova::systemFont());
        theme->apply(initial);
        setTitlebarColorOfAllWindowsOnMac(qApp->palette().window().color());

        m_logColors = theme->logColorScheme();
        m_currentThemeId = name;
        refreshIconTheme();
        watchCurrentTheme();
        emit APPLICATION->themeApplied();
    } else {
        themeWarningLog() << "Tried to set invalid theme:" << name;
    }
}

void ThemeManager::applyCurrentlySelectedTheme(bool initial)
{
    auto* settings = APPLICATION->settings();
    auto applicationTheme = settings->get("ApplicationTheme").toString();
    if (applicationTheme == "") {
        applicationTheme = m_defaultStyle;
    }
    setApplicationTheme(applicationTheme, initial);
    themeDebugLog() << "<> Application theme set.";
    // after the application theme, the Nova icons take its colors
    setIconTheme(settings->get("IconTheme").toString());
    themeDebugLog() << "<> Icon theme set.";
}

QString ThemeManager::getCatPack(const QString& catName)
{
    auto catIter = m_catPacks.find(!catName.isEmpty() ? catName : APPLICATION->settings()->get("BackgroundCat").toString());
    if (catIter != m_catPacks.end()) {
        auto& catPack = catIter->second;
        themeDebugLog() << "applying catpack" << catPack->id();
        return catPack->path();
    }
    themeWarningLog() << "Tried to get invalid catPack:" << catName;

    return m_catPacks.begin()->second->path();
}

QString ThemeManager::addCatPack(std::unique_ptr<CatPack> catPack)
{
    QString id = catPack->id();
    if (!m_catPacks.contains(id)) {
        m_catPacks.emplace(id, std::move(catPack));
    } else {
        themeWarningLog() << "CatPack(" << id << ") not added to prevent id duplication";
    }
    return id;
}

void ThemeManager::initializeCatPacks()
{
    QList<std::pair<QString, QString>> defaultCats{ { "kitteh", QObject::tr("Background Cat (from MultiMC)") },
                                                    { "rory", QObject::tr("Rory ID 11 (drawn by Ashtaka)") },
                                                    { "rory-flat", QObject::tr("Rory ID 11 (flat edition, drawn by Ashtaka)") },
                                                    { "teawie", QObject::tr("Teawie (drawn by SympathyTea)") } };
    for (const auto& [id, name] : defaultCats) {
        addCatPack(std::unique_ptr<CatPack>(new BasicCatPack(id, name)));
    }
    if (!m_catPacksFolder.mkpath(".")) {
        themeWarningLog() << "Couldn't create catpacks folder";
    }
    themeDebugLog() << "CatPacks Folder Path:" << m_catPacksFolder.absolutePath();

    QStringList supportedImageFormats;
    for (const auto& format : QImageReader::supportedImageFormats()) {
        supportedImageFormats.append("*." + format);
    }
    auto loadFiles = [this, supportedImageFormats](const QDir& dir) {
        // Load image files directly
        for (const auto& entry : QDirListing(dir.absoluteFilePath(""), supportedImageFormats,
                                             QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
            QFile customCatFile(entry.absoluteFilePath());
            QFileInfo customCatFileInfo(customCatFile);
            themeDebugLog() << "Loading CatPack from:" << customCatFileInfo.absoluteFilePath();
            addCatPack(std::unique_ptr<CatPack>(new FileCatPack(customCatFileInfo)));
        }
    };

    loadFiles(m_catPacksFolder);

    for (const auto& dirEntry :
         QDirListing(m_catPacksFolder.path(), QDirListing::IteratorFlag::DirsOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
        QDir dir(dirEntry.filePath());
        QFileInfo manifest(dir.absoluteFilePath("catpack.json"));
        if (manifest.isFile()) {
            // Load background manifest
            themeDebugLog() << "Loading background manifest from:" << manifest.absoluteFilePath();
            auto catPack = JsonCatPack::create(manifest);
            if (!catPack) {
                themeWarningLog() << "Couldn't load catpack json:" << catPack.error();
            } else {
                addCatPack(std::move(*catPack));
            }
        } else {
            loadFiles(dir);
        }
    }
}

void ThemeManager::refresh()
{
    m_themes.clear();
    m_icons.clear();
    m_catPacks.clear();

    initializeThemes();
    initializeCatPacks();
}

NovaTheme* ThemeManager::novaTheme(const QString& id)
{
    auto it = m_themes.find(id);
    return it == m_themes.end() ? nullptr : dynamic_cast<NovaTheme*>(it->second.get());
}

NovaTheme* ThemeManager::currentNovaTheme()
{
    return novaTheme(m_currentThemeId);
}

QString ThemeManager::uniqueThemeFolderName(const QString& name)
{
    QString base = name.toLower();
    static const QRegularExpression s_invalid("[^a-z0-9_-]+");
    base.replace(s_invalid, "-");
    base = base.trimmed();
    while (base.startsWith('-')) {
        base.remove(0, 1);
    }
    while (base.endsWith('-')) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        base = "my-theme";
    }
    QString candidate = base;
    for (int i = 2; m_themes.contains(candidate) || m_applicationThemeFolder.exists(candidate); i++) {
        candidate = QString("%1-%2").arg(base).arg(i);
    }
    return candidate;
}

void ThemeManager::watchCurrentTheme()
{
    if (!m_themeWatcher) {
        return;
    }
    if (auto files = m_themeWatcher->files(); !files.isEmpty()) {
        m_themeWatcher->removePaths(files);
    }
    if (auto dirs = m_themeWatcher->directories(); !dirs.isEmpty()) {
        m_themeWatcher->removePaths(dirs);
    }
    auto* theme = currentNovaTheme();
    if (!theme || theme->isBuiltIn()) {
        return;
    }
    QStringList paths{ theme->directory() };
    for (const auto& file : theme->watchedFiles()) {
        if (QFileInfo::exists(file)) {
            paths << file;
        }
    }
    m_themeWatcher->addPaths(paths);
}

void ThemeManager::reloadCurrentTheme()
{
    m_reloadPending = false;
    auto* theme = currentNovaTheme();
    if (!theme || theme->isBuiltIn()) {
        return;
    }
    if (auto result = theme->reload(); !result) {
        themeWarningLog() << "Couldn't reload theme" << theme->id() << ":" << result.error();
        return;
    }
    themeDebugLog() << "Theme files changed, reloading" << theme->id();
    setApplicationTheme(m_currentThemeId);
}
