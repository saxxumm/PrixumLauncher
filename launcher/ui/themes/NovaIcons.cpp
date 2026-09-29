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

#include "NovaIcons.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>

#include "FileSystem.h"
#include "NovaTheme.h"

namespace NovaIcons {

namespace {

class TintedIconEngine : public QIconEngine {
   public:
    TintedIconEngine(QString path, Tint tint) : m_path(std::move(path)), m_tint(tint) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override { return scaledPixmap(size, mode, state, 1.0); }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
        const QSize pixels = (QSizeF(size) * scale).toSize();
        if (pixels.isEmpty()) {
            return {};
        }
        const QColor color = colorFor(mode);
        const QString key = QString("nova-icon:%1:%2x%3:%4").arg(m_path).arg(pixels.width()).arg(pixels.height()).arg(color.rgba(), 0, 16);

        QPixmap result;
        if (!QPixmapCache::find(key, &result)) {
            QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            {
                QPainter p(&image);
                p.setRenderHint(QPainter::Antialiasing);
                QIcon(m_path).paint(&p, QRect(QPoint(), pixels));
                p.setCompositionMode(QPainter::CompositionMode_SourceIn);
                p.fillRect(image.rect(), color);
            }
            result = QPixmap::fromImage(image);
            QPixmapCache::insert(key, result);
        }
        result.setDevicePixelRatio(scale);
        return result;
    }

    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine* clone() const override { return new TintedIconEngine(m_path, m_tint); }
    QString key() const override { return QStringLiteral("NovaTintedIconEngine"); }

   private:
    QColor colorFor(QIcon::Mode mode) const
    {
        const auto tokens = Nova::current();
        if (mode == QIcon::Disabled) {
            return tokens.color("textDisabled");
        }
        switch (m_tint) {
            case Tint::Muted:
                return tokens.color("textMuted");
            case Tint::Accent:
                return tokens.color("accent");
            case Tint::AccentText:
                return tokens.color("accentText");
            case Tint::Danger:
                return tokens.color("danger");
            case Tint::Success:
                return tokens.color("success");
            case Tint::Text:
            default:
                return tokens.color("text");
        }
    }

    QString m_path;
    Tint m_tint;
};

}  // namespace

QIcon icon(const QString& name, Tint tint)
{
    return QIcon(new TintedIconEngine(QString(":/nova/icons/%1.svg").arg(name), tint));
}

namespace {

struct ThemeIcon {
    const char* themeName;
    const char* source;
    const char* color;
};

// names the rest of the launcher asks QIcon::fromTheme for, logos of services are inherited from pe_colored
const ThemeIcon s_themeIcons[] = {
    { "about", "info", "text" },
    { "accounts", "users", "text" },
    { "appearance", "palette", "text" },
    { "bug", "bug", "text" },
    { "cat", "cat", "text" },
    { "centralmods", "puzzle", "text" },
    { "checkupdate", "update", "text" },
    { "copy", "copy", "text" },
    { "coremods", "chip", "text" },
    { "custom-commands", "terminal", "text" },
    { "datapacks", "archive", "text" },
    { "delete", "trash", "text" },
    { "export", "export", "text" },
    { "externaltools", "wrench", "text" },
    { "help", "help", "text" },
    { "instance-settings", "sliders", "text" },
    { "jarmods", "jar", "text" },
    { "java", "coffee", "text" },
    { "language", "languages", "text" },
    { "launch", "play", "text" },
    { "loadermods", "puzzle", "text" },
    { "lock", "lock", "text" },
    { "log", "logs", "text" },
    { "minecraft", "cube", "text" },
    { "new", "plus-square", "text" },
    { "news", "news", "text" },
    { "noaccount", "user-x", "text" },
    { "notes", "note", "text" },
    { "proxy", "network", "text" },
    { "refresh", "refresh", "text" },
    { "rename", "rename", "text" },
    { "resourcepacks", "layers", "text" },
    { "screenshots", "image", "text" },
    { "server", "server", "text" },
    { "settings", "settings", "text" },
    { "shaderpacks", "sun", "text" },
    { "shortcut", "shortcut", "text" },
    { "star", "star", "warning" },
    { "status-bad", "x-circle", "danger" },
    { "status-good", "check-circle", "success" },
    { "status-running", "play-circle", "success" },
    { "status-yellow", "alert", "warning" },
    { "tag", "tag", "text" },
    { "unlock", "unlock", "text" },
    { "viewfolder", "folder", "text" },
    { "worlds", "globe", "text" },
};

}  // namespace

QString prepareIconTheme()
{
    const auto tokens = Nova::current();
    QStringList colors;
    for (const auto* key : { "text", "success", "danger", "warning" }) {
        colors << tokens.color(key).name(QColor::HexRgb);
    }
    QStringList names;
    for (const auto& entry : s_themeIcons) {
        names << QString("%1=%2").arg(entry.themeName, entry.source);
    }
    const QString themeName = "nova-" + QString::number(qHash(colors.join(',') + names.join(',')), 16);
    const QDir base(QDir("cache/nova/icons").absolutePath());
    const QDir dir(base.absoluteFilePath(themeName));

    if (!QFileInfo::exists(dir.absoluteFilePath("index.theme"))) {
        if (!FS::ensureFolderPathExists(dir.absoluteFilePath("scalable"))) {
            qWarning() << "[Theme] Couldn't create the Nova icon theme folder" << dir.absolutePath();
        }
        for (const auto& entry : s_themeIcons) {
            QFile source(QString(":/nova/icons/%1.svg").arg(entry.source));
            if (!source.open(QIODevice::ReadOnly)) {
                qWarning() << "[Theme] Missing Nova icon" << entry.source;
                continue;
            }
            QByteArray svg = source.readAll();
            svg.replace("\"#000\"", ("\"" + tokens.color(entry.color).name(QColor::HexRgb) + "\"").toUtf8());
            if (auto result = FS::write(dir.absoluteFilePath(QString("scalable/%1.svg").arg(entry.themeName)), svg); !result) {
                qWarning() << "[Theme] Couldn't write Nova icon:" << result.error();
            }
        }
        // written last, so a half finished folder is never picked up
        QFile index(":/nova/icontheme/index.theme");
        if (index.open(QIODevice::ReadOnly)) {
            if (auto result = FS::write(dir.absoluteFilePath("index.theme"), index.readAll()); !result) {
                qWarning() << "[Theme] Couldn't write the Nova icon theme index:" << result.error();
            }
        }
    }

    if (!QIcon::themeSearchPaths().contains(base.absolutePath())) {
        QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << base.absolutePath());
    }

    // only keep the theme in use and the new one, colors change often while a theme is edited
    const QString inUse = QIcon::themeName();
    for (const auto& old : base.entryList({ "nova-*" }, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (old != themeName && old != inUse) {
            QDir(base.absoluteFilePath(old)).removeRecursively();
        }
    }
    return themeName;
}

}  // namespace NovaIcons
