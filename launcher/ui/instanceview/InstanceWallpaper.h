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
#include <QImage>
#include <QRectF>

class QPainter;

/**
 * A picture behind the instance list. The instance tiles sit on frosted glass: each one shows a blurred copy of the
 * part of the wallpaper behind it. The wallpaper stays put while the list scrolls, like a window over a backdrop.
 */
class InstanceWallpaper {
   public:
    /// false when the file is not an image Qt can read
    bool load(const QString& path);
    bool isNull() const { return m_source.isNull(); }

    /// strength of the frosted glass, in logical pixels
    void setBlurRadius(int radius);
    /// how much of the theme background covers the picture, 0 to 100, keeps the text readable
    void setDim(int percent, const QColor& background);

    /// scales the picture to cover a viewport of this size, call before painting
    void prepare(const QSize& size, qreal devicePixelRatio);

    /// the picture inside rect, rounded like the frame around the list
    void paintBackground(QPainter* painter, const QRectF& rect, qreal radius) const;
    /// the blurred picture behind rect, rect and the painter in viewport coordinates
    void paintGlass(QPainter* painter, const QRectF& rect, qreal radius) const;
    /// a pane of frosted glass in the colors of the theme, for an instance tile
    void paintTile(QPainter* painter, const QRect& card, qreal radius, bool selected, bool hovered) const;

    /// box blur run three times, which comes close to a gaussian, on a quarter size copy for speed
    static QImage blurred(const QImage& image, int radius);

   private:
    void invalidate();
    void paintImage(QPainter* painter, const QImage& image, const QRectF& rect, qreal radius) const;

    QImage m_source;
    int m_blurRadius = 28;
    int m_dim = 30;
    QColor m_background;

    QSize m_size;
    qreal m_devicePixelRatio = 1;
    QImage m_cover;
    QImage m_glass;
};
