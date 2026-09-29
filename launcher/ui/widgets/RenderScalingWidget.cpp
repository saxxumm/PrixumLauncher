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

#include "RenderScalingWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "minecraft/launch/RenderScaling.h"
#include "settings/SettingsObject.h"

namespace {

quint32 noise(int x, int y, int seed)
{
    quint32 h = static_cast<quint32>(x) * 374761393U + static_cast<quint32>(y) * 668265263U + static_cast<quint32>(seed) * 2246822519U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}

QColor vary(const QColor& color, int x, int y, int seed, int amount)
{
    const int delta = static_cast<int>(noise(x, y, seed) % static_cast<quint32>(amount * 2 + 1)) - amount;
    return color.lighter(100 + delta);
}

// A tiny block world with a HUD, rendered without antialiasing like the game itself.
// Everything is laid out on a 480x200 canvas and scaled to the target image.
void paintScene(QImage& image)
{
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    p.scale(image.width() / 480.0, image.height() / 200.0);

    QLinearGradient sky(0, 0, 0, 200);
    sky.setColorAt(0, QColor(86, 145, 245));
    sky.setColorAt(1, QColor(185, 215, 255));
    p.fillRect(QRectF(0, 0, 480, 200), sky);

    p.fillRect(QRectF(392, 20, 28, 28), QColor(255, 246, 180));
    for (const auto& cloud : { QRectF(40, 26, 64, 12), QRectF(56, 20, 32, 8), QRectF(210, 40, 80, 12), QRectF(230, 34, 40, 8) }) {
        p.fillRect(cloud, QColor(250, 250, 255));
    }

    const QColor grass(95, 159, 53), dirt(134, 96, 67), stone(125, 125, 125), log(102, 81, 51), leaves(58, 122, 36);
    auto block = [&p](int bx, int by, const QColor& base, const QColor& topColor, int seed) {
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                const bool top = topColor.isValid() && (py == 0 || (py == 1 && noise(bx * 4 + px, by, seed) % 3 == 0));
                p.fillRect(QRectF(bx * 16 + px * 4, by * 16 + py * 4, 4, 4),
                           vary(top ? topColor : base, bx * 4 + px, by * 4 + py, seed, 12));
            }
        }
    };

    const int rows = 13;  // 200 / 16, rounded up
    for (int col = 0; col < 30; col++) {
        const int height = 3 + static_cast<int>(std::lround(2.2 + 2.2 * std::sin(col * 0.42) + noise(col, 0, 7) % 2));
        for (int i = 0; i < height; i++) {
            const int row = rows - 1 - i;
            if (i == height - 1) {
                block(col, row, dirt, grass, 1);
            } else if (i < height - 3) {
                block(col, row, stone, QColor(), 2);
            } else {
                block(col, row, dirt, QColor(), 3);
            }
        }
        if (col == 21) {
            const int ground = rows - height;
            for (int t = 1; t <= 3; t++) {
                block(col, ground - t, log, QColor(), 4);
            }
            for (int lx = -1; lx <= 1; lx++) {
                for (int ly = 4; ly <= 5; ly++) {
                    block(col + lx, ground - ly, leaves, QColor(), 5);
                }
            }
        }
    }

    // crosshair
    p.fillRect(QRectF(238, 92, 4, 16), QColor(255, 255, 255, 220));
    p.fillRect(QRectF(232, 98, 16, 4), QColor(255, 255, 255, 220));

    // hotbar
    const QRectF bar(150, 176, 180, 20);
    p.fillRect(bar, QColor(0, 0, 0, 140));
    for (int slot = 0; slot < 9; slot++) {
        const QRectF cell(bar.x() + 2 + slot * 20, bar.y() + 2, 16, 16);
        p.setPen(QPen(slot == 2 ? QColor(255, 255, 255) : QColor(140, 140, 140), 1));
        p.drawRect(cell.adjusted(0, 0, -1, -1));
    }
    p.fillRect(QRectF(bar.x() + 6, bar.y() + 6, 8, 8), grass);
    p.fillRect(QRectF(bar.x() + 26, bar.y() + 6, 8, 8), stone);
    p.fillRect(QRectF(bar.x() + 46, bar.y() + 6, 8, 8), log);

    QFont font;
    font.setPixelSize(11);
    font.setBold(true);
    font.setStyleStrategy(QFont::NoAntialias);
    p.setFont(font);
    p.setPen(QColor(63, 63, 63));
    p.drawText(QPointF(7, 15), "Minecraft 1.21 (60 fps)");
    p.setPen(Qt::white);
    p.drawText(QPointF(6, 14), "Minecraft 1.21 (60 fps)");
}

QImage sharpen(const QImage& source, double amount)
{
    QImage src = source.convertToFormat(QImage::Format_RGB32);
    QImage out(src.size(), QImage::Format_RGB32);
    const int w = src.width(), h = src.height();
    for (int y = 0; y < h; y++) {
        auto* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; x++) {
            int sum[3] = { 0, 0, 0 };
            for (int dy = -1; dy <= 1; dy++) {
                const auto* line = reinterpret_cast<const QRgb*>(src.constScanLine(std::clamp(y + dy, 0, h - 1)));
                for (int dx = -1; dx <= 1; dx++) {
                    const QRgb px = line[std::clamp(x + dx, 0, w - 1)];
                    sum[0] += qRed(px);
                    sum[1] += qGreen(px);
                    sum[2] += qBlue(px);
                }
            }
            const QRgb center = reinterpret_cast<const QRgb*>(src.constScanLine(y))[x];
            auto channel = [amount](int value, int blurSum) {
                return std::clamp(static_cast<int>(std::lround(value + amount * (value - blurSum / 9.0))), 0, 255);
            };
            dst[x] = qRgb(channel(qRed(center), sum[0]), channel(qGreen(center), sum[1]), channel(qBlue(center), sum[2]));
        }
    }
    return out;
}

}  // namespace

RenderScalePreview::RenderScalePreview(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(false);
    setCursor(Qt::SplitHCursor);
    setToolTip(tr("Drag to compare the native image (left) with the scaled one (right)."));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(220);
}

void RenderScalePreview::setScale(int percent, const QString& filter, int sharpness)
{
    if (percent == m_percent && filter == m_filter && sharpness == m_sharpness && !m_scaled.isNull()) {
        return;
    }
    m_percent = percent;
    m_filter = filter;
    m_sharpness = sharpness;
    regenerate();
    update();
}

void RenderScalePreview::regenerate()
{
    const qreal dpr = devicePixelRatioF();
    const QSize full = (QSizeF(size()) * dpr).toSize();
    if (full.isEmpty()) {
        return;
    }

    m_native = QImage(full, QImage::Format_RGB32);
    paintScene(m_native);
    m_native.setDevicePixelRatio(dpr);

    const QSize low(std::max(8, full.width() * m_percent / 100), std::max(4, full.height() * m_percent / 100));
    QImage lowRes(low, QImage::Format_RGB32);
    paintScene(lowRes);

    const bool smooth = m_filter != "nearest";
    m_scaled = lowRes.scaled(full, Qt::IgnoreAspectRatio, smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
    if (m_filter == "fsr" || m_filter == "nis") {
        // rough stand-in for the edge adaptive sharpening those filters do, 0 is the strongest setting
        m_scaled = sharpen(m_scaled, (20 - m_sharpness) / 20.0 * 1.4);
    }
    m_scaled.setDevicePixelRatio(dpr);
}

void RenderScalePreview::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    regenerate();
}

void RenderScalePreview::mousePressEvent(QMouseEvent* event)
{
    mouseMoveEvent(event);
}

void RenderScalePreview::mouseMoveEvent(QMouseEvent* event)
{
    if (width() > 0) {
        m_split = std::clamp(event->position().x() / width(), 0.0, 1.0);
        update();
    }
}

void RenderScalePreview::paintEvent(QPaintEvent*)
{
    if (m_native.isNull() || m_scaled.isNull()) {
        regenerate();
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), 10, 10);
    p.setClipPath(clip);

    const int splitX = static_cast<int>(std::lround(width() * m_split));
    p.drawImage(QRect(0, 0, width(), height()), m_scaled);
    p.save();
    p.setClipRect(QRect(0, 0, splitX, height()), Qt::IntersectClip);
    p.drawImage(QRect(0, 0, width(), height()), m_native);
    p.restore();

    p.setPen(QPen(QColor(255, 255, 255, 230), 2));
    p.drawLine(splitX, 0, splitX, height());
    p.setBrush(QColor(255, 255, 255, 230));
    p.drawEllipse(QPointF(splitX, height() / 2.0), 7, 7);

    auto tag = [&p](const QString& text, bool right, const QRect& area) {
        QFont font = p.font();
        font.setBold(true);
        font.setPixelSize(11);
        p.setFont(font);
        const QFontMetrics metrics(font);
        QRect box(0, 0, metrics.horizontalAdvance(text) + 16, metrics.height() + 8);
        box.moveBottom(area.bottom() - 8);
        if (right) {
            box.moveRight(area.right() - 8);
        } else {
            box.moveLeft(area.left() + 8);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRoundedRect(box, 6, 6);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, text);
    };
    tag(tr("Native"), false, rect());
    tag(tr("%1% scaled").arg(m_percent), true, rect());
}

RenderScalingWidget::RenderScalingWidget(bool instanceMode, QWidget* parent) : QWidget(parent), m_instanceMode(instanceMode)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_group = new QGroupBox(tr("Render &Scaling"), this);
    m_group->setCheckable(instanceMode);
    outer->addWidget(m_group);
    outer->addStretch(1);

    auto* layout = new QVBoxLayout(m_group);

    auto* intro = new QLabel(tr("Render the game at a lower resolution and upscale it to the window size. Use it to gain FPS on weak "
                                "hardware or to get a chunky retro look. Choose a sharp filter for crisp pixels or a smooth one "
                                "for a soft, blurred image."),
                             m_group);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* statusRow = new QHBoxLayout;
    m_status = new QLabel(m_group);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setObjectName("renderScalingStatus");
    m_recheck = new QPushButton(tr("Check again"), m_group);
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_recheck, 0, Qt::AlignTop);
    layout->addLayout(statusRow);

    m_enabled = new QCheckBox(tr("&Enable render scaling"), m_group);
    layout->addWidget(m_enabled);

    m_options = new QWidget(m_group);
    auto* form = new QFormLayout(m_options);
    form->setContentsMargins(0, 0, 0, 0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->addWidget(m_options);

    // resolution
    {
        auto* column = new QVBoxLayout;
        auto* row = new QHBoxLayout;
        m_percentSlider = new QSlider(Qt::Horizontal, m_options);
        m_percentSlider->setRange(RenderScaling::s_minPercent, RenderScaling::s_maxPercent);
        m_percentSpin = new QSpinBox(m_options);
        m_percentSpin->setRange(RenderScaling::s_minPercent, RenderScaling::s_maxPercent);
        m_percentSpin->setSuffix(" %");
        row->addWidget(m_percentSlider, 1);
        row->addWidget(m_percentSpin);
        column->addLayout(row);

        auto* presets = new QHBoxLayout;
        for (int preset : { 25, 33, 50, 67, 75 }) {
            auto* button = new QPushButton(QString("%1%").arg(preset), m_options);
            button->setProperty("novaRole", "chip");
            button->setFocusPolicy(Qt::TabFocus);
            connect(button, &QPushButton::clicked, this, [this, preset] { m_percentSpin->setValue(preset); });
            presets->addWidget(button);
        }
        presets->addStretch(1);
        column->addLayout(presets);

        m_resolution = new QLabel(m_options);
        m_resolution->setWordWrap(true);
        m_resolution->setObjectName("renderScalingResolution");
        column->addWidget(m_resolution);
        form->addRow(tr("Render &resolution:"), column);
        qobject_cast<QLabel*>(form->labelForField(column))->setBuddy(m_percentSpin);

        connect(m_percentSlider, &QSlider::valueChanged, m_percentSpin, &QSpinBox::setValue);
        connect(m_percentSpin, &QSpinBox::valueChanged, m_percentSlider, &QSlider::setValue);
        connect(m_percentSpin, &QSpinBox::valueChanged, this, &RenderScalingWidget::updateState);
    }

    // filter
    {
        m_filter = new QComboBox(m_options);
        m_filter->addItem(tr("Sharp pixels: nearest neighbour, no blur"), "nearest");
        m_filter->addItem(tr("Smooth: bilinear, soft blur"), "linear");
        m_filter->addItem(tr("AMD FidelityFX Super Resolution (sharpened)"), "fsr");
        m_filter->addItem(tr("NVIDIA Image Scaling (sharpened)"), "nis");
        m_filter->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_filter->setMinimumContentsLength(16);
        form->addRow(tr("Upscaling &filter:"), m_filter);
        connect(m_filter, &QComboBox::currentIndexChanged, this, &RenderScalingWidget::updateState);

        m_sharpness = new QSlider(Qt::Horizontal, m_options);
        m_sharpness->setRange(0, 20);
        m_sharpness->setInvertedAppearance(true);
        m_sharpness->setToolTip(tr("Sharpening strength of FSR and NIS. Left is softer, right is sharper."));
        form->addRow(tr("S&harpness:"), m_sharpness);
        m_sharpnessLabel = qobject_cast<QLabel*>(form->labelForField(m_sharpness));
        connect(m_sharpness, &QSlider::valueChanged, this, &RenderScalingWidget::updateState);
    }

    // presentation
    {
        m_scaler = new QComboBox(m_options);
        m_scaler->addItem(tr("Fit, keep aspect ratio"), "fit");
        m_scaler->addItem(tr("Integer, pixel perfect multiples only"), "integer");
        m_scaler->addItem(tr("Stretch to fill the window"), "stretch");
        m_scaler->addItem(tr("Fill, crop the edges"), "fill");
        m_scaler->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_scaler->setMinimumContentsLength(16);
        form->addRow(tr("Scaling &mode:"), m_scaler);

        m_fullscreen = new QCheckBox(tr("Start in f&ullscreen"), m_options);
        m_fullscreen->setToolTip(tr("gamescope opens in fullscreen and the screen resolution is used as the output size."));
        connect(m_fullscreen, &QCheckBox::toggled, this, &RenderScalingWidget::updateState);
        form->addRow(QString(), m_fullscreen);

        m_grabCursor = new QCheckBox(tr("Lock the mouse &cursor to the game"), m_options);
        m_grabCursor->setToolTip(tr("Passes --force-grab-cursor to gamescope. Without it the camera may spin when moving the mouse."));
        form->addRow(QString(), m_grabCursor);

        m_extraArgs = new QLineEdit(m_options);
        m_extraArgs->setPlaceholderText(tr("e.g. -r 60 --adaptive-sync"));
        m_extraArgs->setToolTip(tr("Additional arguments that are passed to gamescope as is."));
        form->addRow(tr("E&xtra gamescope arguments:"), m_extraArgs);
    }

    m_preview = new RenderScalePreview(m_group);
    layout->addWidget(m_preview);

    connect(m_enabled, &QCheckBox::toggled, this, &RenderScalingWidget::updateState);
    connect(m_group, &QGroupBox::toggled, this, &RenderScalingWidget::updateState);
    connect(m_recheck, &QPushButton::clicked, this, &RenderScalingWidget::refreshGamescopeStatus);

    refreshGamescopeStatus();
}

void RenderScalingWidget::refreshGamescopeStatus()
{
    if (!RenderScaling::isPlatformSupported()) {
        m_status->setText(tr("Render scaling relies on gamescope, which is only available on Linux."));
        m_status->setProperty("state", "warning");
        m_recheck->hide();
        m_group->setEnabled(false);
    } else if (const auto path = RenderScaling::findGamescope(); path.isEmpty()) {
        m_status->setText(
            tr("<b>gamescope was not found.</b> Render scaling needs it to upscale the game. Install it from your "
               "distribution's repositories (for example <code>sudo pacman -S gamescope</code>, "
               "<code>sudo apt install gamescope</code> or <code>sudo dnf install gamescope</code>), then press "
               "\"Check again\"."));
        m_status->setProperty("state", "warning");
        m_recheck->show();
    } else {
        m_status->setText(tr("gamescope found: %1").arg(path));
        m_status->setProperty("state", "ok");
        m_recheck->hide();
    }
    // re-evaluate [state] selectors of the stylesheet
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

void RenderScalingWidget::setWindowSize(const QSize& size, bool maximized)
{
    m_windowSize = size;
    m_maximized = maximized;
    updateState();
}

void RenderScalingWidget::updateState()
{
    m_options->setEnabled(m_enabled->isChecked());

    const auto filter = m_filter->currentData().toString();
    const bool sharpening = filter == "fsr" || filter == "nis";
    m_sharpness->setVisible(sharpening);
    m_sharpnessLabel->setVisible(sharpening);

    QSize output = m_windowSize;
    if (auto* screen = QGuiApplication::primaryScreen()) {
        if (m_fullscreen->isChecked()) {
            output = (QSizeF(screen->size()) * screen->devicePixelRatio()).toSize();
        } else if (m_maximized) {
            output = (QSizeF(screen->availableSize()) * screen->devicePixelRatio()).toSize();
        }
    }
    const auto internal = RenderScaling::internalSize(output, m_percentSpin->value());
    QString text = tr("The game renders at <b>%1 × %2</b> and is shown at %3 × %4.")
                       .arg(internal.width())
                       .arg(internal.height())
                       .arg(output.width())
                       .arg(output.height());
    if (filter == "nearest" && 100 % m_percentSpin->value() != 0 && m_percentSpin->value() != 33) {
        text += "<br>" + tr("Tip: 50%, 33% or 25% keep every pixel the same size.");
    }
    m_resolution->setText(text);

    m_preview->setScale(m_percentSpin->value(), filter, m_sharpness->value());
}

void RenderScalingWidget::loadSettings(SettingsObject* settings)
{
    const auto config = RenderScaling::readConfig(settings);
    m_group->setChecked(!m_instanceMode || settings->get("OverrideRenderScale").toBool());
    m_enabled->setChecked(config.enabled);
    m_percentSpin->setValue(config.percent);
    m_filter->setCurrentIndex(std::max(0, m_filter->findData(config.filter)));
    m_scaler->setCurrentIndex(std::max(0, m_scaler->findData(config.scaler)));
    m_sharpness->setValue(config.sharpness);
    m_fullscreen->setChecked(config.fullscreen);
    m_grabCursor->setChecked(config.grabCursor);
    m_extraArgs->setText(config.extraArgs);
    updateState();
}

void RenderScalingWidget::saveSettings(SettingsObject* settings)
{
    const bool override = !m_instanceMode || m_group->isChecked();
    if (m_instanceMode) {
        settings->set("OverrideRenderScale", override);
    }
    if (override) {
        settings->set("RenderScaleEnabled", m_enabled->isChecked());
        settings->set("RenderScalePercent", m_percentSpin->value());
        settings->set("RenderScaleFilter", m_filter->currentData().toString());
        settings->set("RenderScaleMode", m_scaler->currentData().toString());
        settings->set("RenderScaleSharpness", m_sharpness->value());
        settings->set("RenderScaleFullscreen", m_fullscreen->isChecked());
        settings->set("RenderScaleGrabCursor", m_grabCursor->isChecked());
        settings->set("RenderScaleExtraArgs", m_extraArgs->text().trimmed());
    } else {
        for (const auto* name : { "RenderScaleEnabled", "RenderScalePercent", "RenderScaleFilter", "RenderScaleMode",
                                  "RenderScaleSharpness", "RenderScaleFullscreen", "RenderScaleGrabCursor", "RenderScaleExtraArgs" }) {
            settings->reset(name);
        }
    }
}
