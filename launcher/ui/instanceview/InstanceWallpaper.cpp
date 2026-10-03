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

#include "InstanceWallpaper.h"

#include <QImageReader>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <utility>
#include "ui/themes/NovaTheme.h"

namespace {

// big photos are scaled down once, a screen never needs more
constexpr QSize s_maxSourceSize(3840, 2400);

template <bool Rows>
void boxBlur(const QImage& source, QImage& target, int radius)
{
    int length = source.height();
    int lines = source.width();
    if constexpr (Rows) {
        std::swap(length, lines);
    }
    const int window = 2 * radius + 1;
    auto pixel = [&](int line, int position) {
        position = std::clamp(position, 0, length - 1);
        if constexpr (Rows) {
            return reinterpret_cast<const QRgb*>(source.constScanLine(line))[position];
        } else {
            return reinterpret_cast<const QRgb*>(source.constScanLine(position))[line];
        }
    };
    for (int line = 0; line < lines; line++) {
        int sum[4] = {};
        auto add = [&sum](QRgb p, int sign) {
            sum[0] += sign * qRed(p);
            sum[1] += sign * qGreen(p);
            sum[2] += sign * qBlue(p);
            sum[3] += sign * qAlpha(p);
        };
        for (int i = -radius; i <= radius; i++) {
            add(pixel(line, i), 1);
        }
        for (int position = 0; position < length; position++) {
            const QRgb value = qRgba(sum[0] / window, sum[1] / window, sum[2] / window, sum[3] / window);
            if constexpr (Rows) {
                reinterpret_cast<QRgb*>(target.scanLine(line))[position] = value;
            } else {
                reinterpret_cast<QRgb*>(target.scanLine(position))[line] = value;
            }
            add(pixel(line, position + radius + 1), 1);
            add(pixel(line, position - radius), -1);
        }
    }
}

}  // namespace

bool InstanceWallpaper::load(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        m_source = {};
        invalidate();
        return false;
    }
    if (image.width() > s_maxSourceSize.width() || image.height() > s_maxSourceSize.height()) {
        image = image.scaled(s_maxSourceSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    invalidate();
    return true;
}

void InstanceWallpaper::setBlurRadius(int radius)
{
    if (radius != m_blurRadius) {
        m_blurRadius = radius;
        invalidate();
    }
}

void InstanceWallpaper::setDim(int percent, const QColor& background)
{
    if (percent != m_dim || background != m_background) {
        m_dim = percent;
        m_background = background;
        invalidate();
    }
}

void InstanceWallpaper::invalidate()
{
    m_size = {};
    m_cover = {};
    m_glass = {};
}

void InstanceWallpaper::prepare(const QSize& size, qreal devicePixelRatio)
{
    if (m_source.isNull() || size.isEmpty() || (size == m_size && qFuzzyCompare(devicePixelRatio, m_devicePixelRatio))) {
        return;
    }
    m_size = size;
    m_devicePixelRatio = devicePixelRatio;

    // cover the viewport like CSS background-size: cover, centered
    const QSize pixels = (QSizeF(size) * devicePixelRatio).toSize();
    QImage cover = m_source.scaled(pixels, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    cover = cover.copy((cover.width() - pixels.width()) / 2, (cover.height() - pixels.height()) / 2, pixels.width(), pixels.height());
    if (m_dim > 0 && m_background.isValid()) {
        QPainter painter(&cover);
        QColor veil = m_background;
        veil.setAlphaF(static_cast<float>(std::clamp(m_dim, 0, 100)) / 100.0F);
        painter.fillRect(cover.rect(), veil);
    }
    m_glass = blurred(cover, static_cast<int>(std::lround(m_blurRadius * devicePixelRatio)));
    m_cover = cover;
}

void InstanceWallpaper::paintBackground(QPainter* painter, const QRectF& rect, qreal radius) const
{
    paintImage(painter, m_cover, rect, radius);
}

void InstanceWallpaper::paintGlass(QPainter* painter, const QRectF& rect, qreal radius) const
{
    paintImage(painter, m_glass, rect, radius);
}

void InstanceWallpaper::paintTile(QPainter* painter, const QRect& card, qreal radius, bool selected, bool hovered) const
{
    const auto tokens = Nova::current();
    const QRectF glass = QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5);
    paintGlass(painter, glass, radius);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QColor tint = tokens.color("surface");
    tint.setAlphaF(hovered || selected ? 0.5F : 0.32F);
    painter->setPen(Qt::NoPen);
    painter->setBrush(tint);
    painter->drawRoundedRect(glass, radius, radius);
    if (selected) {
        QColor accent = tokens.color("accent");
        accent.setAlphaF(0.22F);
        painter->setBrush(accent);
        painter->drawRoundedRect(glass, radius, radius);
    }
    // light catches the top edge of the pane
    QLinearGradient sheen(glass.topLeft(), glass.bottomLeft());
    sheen.setColorAt(0, QColor(255, 255, 255, 30));
    sheen.setColorAt(0.45, QColor(255, 255, 255, 6));
    sheen.setColorAt(1, QColor(255, 255, 255, 0));
    painter->setBrush(sheen);
    painter->drawRoundedRect(glass, radius, radius);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(selected ? QPen(tokens.color("accent"), 1.5) : QPen(QColor(255, 255, 255, hovered ? 70 : 42), 1));
    painter->drawRoundedRect(selected ? QRectF(card).adjusted(0.75, 0.75, -0.75, -0.75) : glass, radius, radius);
    painter->restore();
}

void InstanceWallpaper::paintImage(QPainter* painter, const QImage& image, const QRectF& rect, qreal radius) const
{
    if (image.isNull()) {
        return;
    }
    // a texture brush lines up with the viewport, which is where the picture lies, and keeps the corners smooth
    QBrush brush(image);
    brush.setTransform(QTransform::fromScale(1 / m_devicePixelRatio, 1 / m_devicePixelRatio));
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(brush);
    painter->drawRoundedRect(rect, radius, radius);
    painter->restore();
}

QImage InstanceWallpaper::blurred(const QImage& image, int radius)
{
    if (radius <= 0 || image.isNull()) {
        return image;
    }
    const int factor = radius >= 12 ? 4 : (radius >= 6 ? 2 : 1);
    const QSize small(std::max(1, image.width() / factor), std::max(1, image.height() / factor));
    QImage work = image.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage temp(work.size(), work.format());
    // three box passes of radius r spread about as far as a gaussian with sigma r
    const int boxRadius = std::max(1, radius / factor);
    for (int pass = 0; pass < 3; pass++) {
        boxBlur<true>(work, temp, boxRadius);
        boxBlur<false>(temp, work, boxRadius);
    }
    return work.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
