// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2025 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (C) 2022 Tayou <git@tayou.org>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "AppearanceWidget.h"
#include "ui/dialogs/ThemeEditorDialog.h"
#include "ui/themes/NovaIcons.h"
#include "ui_AppearanceWidget.h"

#include <DesktopServices.h>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QImageReader>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStandardPaths>
#include <cmath>
#include "BuildConfig.h"
#include "icons/IconList.h"
#include "ui/instanceview/InstanceWallpaper.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/NovaTheme.h"
#include "ui/themes/ThemeManager.h"

#include <Application.h>
#include "settings/SettingsObject.h"

namespace {

const QSize s_themeCardSize(132, 84);

/// a tiny window in the colors of the theme: sidebar, a card with tiles and an accent button
QPixmap themePreview(ITheme* theme, qreal devicePixelRatio)
{
    const QPalette palette = theme->colorScheme();
    QColor window = palette.color(QPalette::Window);
    QColor sidebar = window.lightness() < 128 ? window.darker(125) : window.darker(104);
    QColor surface = palette.color(QPalette::Base);
    QColor text = palette.color(QPalette::WindowText);
    QColor accent = palette.color(QPalette::Highlight);
    if (auto* nova = dynamic_cast<NovaTheme*>(theme)) {
        const auto& tokens = nova->tokens();
        window = tokens.color("window");
        sidebar = tokens.color("sidebar");
        surface = tokens.color("surface");
        text = tokens.color("text");
        accent = tokens.color("accent");
    }

    QPixmap pixmap(s_themeCardSize * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = QRectF(QPointF(0, 0), QSizeF(s_themeCardSize)).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(bounds, 8, 8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(window);
    painter.drawPath(shape);
    painter.setClipPath(shape);

    QColor faint = text;
    faint.setAlphaF(0.3F);
    const QRectF side(0, 0, bounds.width() * 0.28, bounds.height());
    painter.fillRect(side, sidebar);
    for (int i = 0; i < 4; i++) {
        QColor color = faint;
        if (i == 0) {
            color = accent;
            color.setAlphaF(0.45F);
        }
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(side.left() + 6, 10 + i * 11, side.width() - 12, 6), 3, 3);
    }

    const QRectF card(side.right() + 7, 7, bounds.width() - side.width() - 14, bounds.height() - 14);
    painter.setBrush(surface);
    painter.drawRoundedRect(card, 5, 5);
    const qreal tileWidth = (card.width() - 12 - 8) / 3;
    QColor tile = text;
    tile.setAlphaF(0.1F);
    painter.setBrush(tile);
    for (int i = 0; i < 3; i++) {
        painter.drawRoundedRect(QRectF(card.left() + 6 + i * (tileWidth + 4), card.top() + 6, tileWidth, card.height() * 0.42), 3, 3);
    }
    painter.setBrush(faint);
    painter.drawRoundedRect(QRectF(card.left() + 6, card.top() + card.height() * 0.42 + 11, card.width() * 0.55, 5), 2.5, 2.5);
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(card.left() + 6, card.bottom() - 17, card.width() * 0.42, 11), 5.5, 5.5);

    painter.setClipping(false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(128, 128, 128, 90), 1));
    painter.drawPath(shape);
    return pixmap;
}

}  // namespace

AppearanceWidget::AppearanceWidget(bool themesOnly, QWidget* parent)
    : QWidget(parent), m_ui(new Ui::AppearanceWidget), m_themesOnly(themesOnly)
{
    m_ui->setupUi(this);

    connect(m_ui->enableCatCheckBox, &QCheckBox::toggled, m_ui->catSettingsBox, &QWidget::setEnabled);
    connect(m_ui->wallpaperCheckBox, &QCheckBox::toggled, m_ui->wallpaperSettings, &QWidget::setEnabled);
    // long cat pack and icon theme names would otherwise make the page wider than the window
    for (auto* combo : { m_ui->catPackComboBox, m_ui->iconsComboBox }) {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(14);
    }

    // themes are picked from cards, the combo box only keeps the list for the code that applies them
    m_ui->widgetStyleLabel->hide();
    m_ui->widgetStyleComboBox->hide();
    auto* cards = m_ui->themeCards;
    cards->setObjectName("themeCards");
    cards->setViewMode(QListView::IconMode);
    cards->setMovement(QListView::Static);
    cards->setResizeMode(QListView::Adjust);
    cards->setWrapping(true);
    cards->setUniformItemSizes(true);
    cards->setIconSize(s_themeCardSize);
    cards->setGridSize(QSize(s_themeCardSize.width() + 22, s_themeCardSize.height() + 40));
    cards->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cards->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cards->setSelectionMode(QAbstractItemView::SingleSelection);
    cards->setFrameShape(QFrame::NoFrame);
    cards->installEventFilter(this);
    cards->viewport()->installEventFilter(this);
    connect(cards, &QListWidget::currentRowChanged, m_ui->widgetStyleComboBox, [this](int row) {
        if (row >= 0) {
            m_ui->widgetStyleComboBox->setCurrentIndex(row);
        }
    });

    m_ui->catPreview->setGraphicsEffect(new QGraphicsOpacityEffect(this));

    m_defaultFormat = QTextCharFormat(m_ui->consolePreview->currentCharFormat());

    if (themesOnly) {
        m_ui->wallpaperBox->hide();
        m_ui->catBox->hide();
        m_ui->settingsBox->hide();
        loadThemeSettings();
    } else {
        loadSettings();
        loadThemeSettings();

        updateConsolePreview();
        updateCatPreview();
    }

    connect(m_ui->fontSizeBox, &QSpinBox::valueChanged, this, &AppearanceWidget::updateConsolePreview);
    connect(m_ui->consoleFont, &QFontComboBox::currentFontChanged, this, &AppearanceWidget::updateConsolePreview);

    connect(m_ui->iconsComboBox, &QComboBox::currentIndexChanged, this, &AppearanceWidget::applyIconTheme);
    connect(m_ui->widgetStyleComboBox, &QComboBox::currentIndexChanged, this, &AppearanceWidget::applyWidgetTheme);
    connect(m_ui->catPackComboBox, &QComboBox::currentIndexChanged, this, &AppearanceWidget::applyCatTheme);
    connect(m_ui->catOpacitySlider, &QAbstractSlider::valueChanged, this, &AppearanceWidget::updateCatPreview);
    connect(m_ui->catSizeSlider, &QAbstractSlider::valueChanged, this, &AppearanceWidget::updateCatPreview);
    connect(m_ui->wallpaperBrowse, &QPushButton::clicked, this, &AppearanceWidget::chooseWallpaper);
    connect(m_ui->wallpaperPath, &QLineEdit::editingFinished, this, &AppearanceWidget::updateWallpaperPreview);
    connect(m_ui->wallpaperBlurSlider, &QAbstractSlider::valueChanged, this, &AppearanceWidget::updateWallpaperPreview);
    connect(m_ui->wallpaperDimSlider, &QAbstractSlider::valueChanged, this, &AppearanceWidget::updateWallpaperPreview);

    connect(m_ui->iconsFolder, &QPushButton::clicked, this,
            [] { DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path()); });
    connect(m_ui->widgetStyleFolder, &QPushButton::clicked, this,
            [] { DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path()); });
    connect(m_ui->catPackFolder, &QPushButton::clicked, this,
            [] { DesktopServices::openPath(APPLICATION->themeManager()->getCatPacksFolder().path()); });
    connect(m_ui->reloadThemesButton, &QPushButton::pressed, this, &AppearanceWidget::loadThemeSettings);

    // the theme editor creates and changes Nova themes
    auto* editThemeButton = new QPushButton(NovaIcons::icon("sparkles"), tr("Theme &Editor..."), this);
    editThemeButton->setToolTip(tr("Create or change a theme and see the result immediately."));
    m_ui->themeButtons->insertWidget(m_ui->themeButtons->indexOf(m_ui->reloadThemesButton) + 1, editThemeButton);
    connect(editThemeButton, &QPushButton::clicked, this, [this] {
        ThemeEditorDialog dialog(this);
        dialog.exec();
        loadThemeSettings();
    });
}

AppearanceWidget::~AppearanceWidget()
{
    delete m_ui;
}

void AppearanceWidget::applySettings()
{
    SettingsObject* settings = APPLICATION->settings();
    QString consoleFontFamily = m_ui->consoleFont->currentFont().family();
    settings->set("ConsoleFont", consoleFontFamily);
    settings->set("ConsoleFontSize", m_ui->fontSizeBox->value());
    const bool catEnabled = m_ui->enableCatCheckBox->isChecked();
    settings->set("EnableCat", catEnabled);
    if (!catEnabled) {
        settings->set("TheCat", false);
    }
    settings->set("CatOpacity", m_ui->catOpacitySlider->value());
    settings->set("CatSize", m_ui->catSizeSlider->value());
    auto catFit = m_ui->catFitComboBox->currentIndex();
    settings->set("CatFit", catFit == 0 ? "fit" : catFit == 1 ? "fill" : "strech");

    settings->set("InstanceWallpaperEnabled", m_ui->wallpaperCheckBox->isChecked());
    settings->set("InstanceWallpaper", m_ui->wallpaperPath->text().trimmed());
    settings->set("InstanceWallpaperBlur", m_ui->wallpaperBlurSlider->value());
    settings->set("InstanceWallpaperDim", m_ui->wallpaperDimSlider->value());
}

void AppearanceWidget::loadSettings()
{
    SettingsObject* settings = APPLICATION->settings();
    QString fontFamily = settings->get("ConsoleFont").toString();
    QFont consoleFont(fontFamily);
    m_ui->consoleFont->setCurrentFont(consoleFont);

    bool conversionOk = true;
    int fontSize = settings->get("ConsoleFontSize").toInt(&conversionOk);
    if (!conversionOk) {
        fontSize = 11;
    }
    m_ui->fontSizeBox->setValue(fontSize);

    m_ui->enableCatCheckBox->setChecked(settings->get("EnableCat").toBool());
    m_ui->catSettingsBox->setEnabled(m_ui->enableCatCheckBox->isChecked());
    m_ui->catOpacitySlider->setValue(settings->get("CatOpacity").toInt());
    m_ui->catSizeSlider->setValue(settings->get("CatSize").toInt());

    m_ui->wallpaperCheckBox->setChecked(settings->get("InstanceWallpaperEnabled").toBool());
    m_ui->wallpaperSettings->setEnabled(m_ui->wallpaperCheckBox->isChecked());
    m_ui->wallpaperPath->setText(settings->get("InstanceWallpaper").toString());
    m_ui->wallpaperBlurSlider->setValue(settings->get("InstanceWallpaperBlur").toInt());
    m_ui->wallpaperDimSlider->setValue(settings->get("InstanceWallpaperDim").toInt());
    updateWallpaperPreview();

    auto catFit = settings->get("CatFit").toString();
    m_ui->catFitComboBox->setCurrentIndex(catFit == "fit" ? 0 : catFit == "fill" ? 1 : 2);
}

void AppearanceWidget::retranslateUi()
{
    m_ui->retranslateUi(this);
}

void AppearanceWidget::applyIconTheme(int index)
{
    auto settings = APPLICATION->settings();
    auto originalIconTheme = settings->get("IconTheme").toString();
    auto newIconTheme = m_ui->iconsComboBox->itemData(index).toString();
    if (originalIconTheme != newIconTheme) {
        settings->set("IconTheme", newIconTheme);
        APPLICATION->themeManager()->applyCurrentlySelectedTheme();
    }
}

void AppearanceWidget::applyWidgetTheme(int index)
{
    auto settings = APPLICATION->settings();
    auto originalAppTheme = settings->get("ApplicationTheme").toString();
    auto newAppTheme = m_ui->widgetStyleComboBox->itemData(index).toString();
    if (originalAppTheme != newAppTheme) {
        settings->set("ApplicationTheme", newAppTheme);
        APPLICATION->themeManager()->applyCurrentlySelectedTheme();
    }

    updateConsolePreview();
}

void AppearanceWidget::applyCatTheme(int index)
{
    auto settings = APPLICATION->settings();
    auto originalCat = settings->get("BackgroundCat").toString();
    auto newCat = m_ui->catPackComboBox->itemData(index).toString();
    if (originalCat != newCat) {
        settings->set("BackgroundCat", newCat);
    }

    APPLICATION->currentCatChanged(index);
    updateCatPreview();
}

void AppearanceWidget::loadThemeSettings()
{
    APPLICATION->themeManager()->refresh();

    m_ui->iconsComboBox->blockSignals(true);
    m_ui->widgetStyleComboBox->blockSignals(true);
    m_ui->catPackComboBox->blockSignals(true);

    m_ui->iconsComboBox->clear();
    m_ui->widgetStyleComboBox->clear();
    m_ui->catPackComboBox->clear();

    SettingsObject* settings = APPLICATION->settings();

    const QString currentIconTheme = settings->get("IconTheme").toString();
    const auto iconThemes = APPLICATION->themeManager()->getValidIconThemes();

    for (int i = 0; i < iconThemes.count(); ++i) {
        const IconTheme* theme = iconThemes[i];

        QIcon iconForComboBox =
            theme->id() == NovaIcons::s_iconThemeId ? NovaIcons::icon("settings") : QIcon(theme->path() + "/scalable/settings");
        // the built-in theme gets its name from a file, which no translation reaches
        const QString name = theme->id() == NovaIcons::s_iconThemeId ? tr("Nova (follows the theme colors)") : theme->name();
        m_ui->iconsComboBox->addItem(iconForComboBox, name, theme->id());

        if (currentIconTheme == theme->id())
            m_ui->iconsComboBox->setCurrentIndex(i);
    }

    const QString currentTheme = settings->get("ApplicationTheme").toString();
    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();
    QSignalBlocker cardsBlocker(m_ui->themeCards);
    m_ui->themeCards->clear();
    for (int i = 0; i < themes.count(); ++i) {
        ITheme* theme = themes[i];

        m_ui->widgetStyleComboBox->addItem(theme->name(), theme->id());

        if (!theme->tooltip().isEmpty())
            m_ui->widgetStyleComboBox->setItemData(i, theme->tooltip(), Qt::ToolTipRole);

        auto* card = new QListWidgetItem(QIcon(themePreview(theme, devicePixelRatioF())), theme->name(), m_ui->themeCards);
        card->setToolTip(theme->tooltip());
        card->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);

        if (currentTheme == theme->id()) {
            m_ui->widgetStyleComboBox->setCurrentIndex(i);
            m_ui->themeCards->setCurrentRow(i);
        }
    }
    fitThemeCards();

    if (!m_themesOnly) {
        const QString currentCat = settings->get("BackgroundCat").toString();
        const auto cats = APPLICATION->themeManager()->getValidCatPacks();
        for (int i = 0; i < cats.count(); ++i) {
            const CatPack* cat = cats[i];

            QIcon catIcon = QIcon(QString("%1").arg(cat->path()));
            m_ui->catPackComboBox->addItem(catIcon, cat->name(), cat->id());

            if (currentCat == cat->id())
                m_ui->catPackComboBox->setCurrentIndex(i);
        }
    }

    m_ui->iconsComboBox->blockSignals(false);
    m_ui->widgetStyleComboBox->blockSignals(false);
    m_ui->catPackComboBox->blockSignals(false);
}

void AppearanceWidget::updateConsolePreview()
{
    const LogColors& colors = APPLICATION->themeManager()->getLogColors();

    int fontSize = m_ui->fontSizeBox->value();
    QString fontFamily = m_ui->consoleFont->currentFont().family();
    m_ui->consolePreview->clear();
    m_defaultFormat.setFont(QFont(fontFamily, fontSize));

    auto print = [this, colors](const QString& message, MessageLevel level) {
        QTextCharFormat format(m_defaultFormat);

        QColor bg = colors.background.value(level);
        QColor fg = colors.foreground.value(level);

        if (bg.isValid())
            format.setBackground(bg);

        if (fg.isValid())
            format.setForeground(fg);

        // append a paragraph/line
        auto workCursor = m_ui->consolePreview->textCursor();
        workCursor.movePosition(QTextCursor::End);
        workCursor.insertText(message, format);
        workCursor.insertBlock();
    };

    print(QString("%1 version: %2\n").arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.printableVersionString()), MessageLevel::Launcher);

    QDate today = QDate::currentDate();

    if (today.month() == 10 && today.day() == 31)
        print(tr("[ERROR] OOoooOOOoooo! A spooky error!"), MessageLevel::Error);
    else
        print(tr("[ERROR] A spooky error!"), MessageLevel::Error);

    print(tr("[INFO] A harmless message..."), MessageLevel::Info);
    print(tr("[WARN] A not so spooky warning."), MessageLevel::Warning);
    print(tr("[DEBUG] A secret debugging message..."), MessageLevel::Debug);
    print(tr("[FATAL] A terrifying fatal error!"), MessageLevel::Fatal);
}

void AppearanceWidget::updateCatPreview()
{
    QIcon catPackIcon(APPLICATION->themeManager()->getCatPack());
    m_ui->catPreview->setIcon(catPackIcon);
    // the preview grows with the size setting, within reason
    const double size = m_ui->catSizeSlider->value() / 100.0;
    const QSize iconSize = (QSizeF(72, 96) * std::clamp(size, 0.5, 2.0)).toSize();
    m_ui->catPreview->setIconSize(iconSize);
    m_ui->catPreview->setFixedWidth(144 + 8);

    m_ui->catOpacityValue->setText(tr("%1%").arg(m_ui->catOpacitySlider->value()));
    m_ui->catSizeValue->setText(tr("%1%").arg(m_ui->catSizeSlider->value()));

    auto effect = dynamic_cast<QGraphicsOpacityEffect*>(m_ui->catPreview->graphicsEffect());
    if (effect)
        effect->setOpacity(m_ui->catOpacitySlider->value() / 100.0);
}

void AppearanceWidget::chooseWallpaper()
{
    QStringList formats;
    for (const auto& format : QImageReader::supportedImageFormats()) {
        formats << "*." + QString::fromLatin1(format);
    }
    QString start = m_ui->wallpaperPath->text();
    if (start.isEmpty()) {
        start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose a Wallpaper"), start, tr("Images (%1)").arg(formats.join(' ')));
    if (path.isEmpty()) {
        return;
    }
    m_ui->wallpaperPath->setText(path);
    m_ui->wallpaperCheckBox->setChecked(true);
    updateWallpaperPreview();
}

void AppearanceWidget::updateWallpaperPreview()
{
    m_ui->wallpaperBlurValue->setText(tr("%1 px").arg(m_ui->wallpaperBlurSlider->value()));
    m_ui->wallpaperDimValue->setText(tr("%1%").arg(m_ui->wallpaperDimSlider->value()));

    const QSize size(320, 180);
    const qreal ratio = devicePixelRatioF();
    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto tokens = Nova::current();
    const qreal radius = std::min(tokens.metric("radius"), 16);

    InstanceWallpaper wallpaper;
    const QString path = m_ui->wallpaperPath->text().trimmed();
    if (path.isEmpty() || !wallpaper.load(path)) {
        // an empty frame that says what goes there
        QPen dashed(tokens.color("borderStrong"), 1.5, Qt::DashLine);
        painter.setPen(dashed);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(1, 1, size.width() - 2, size.height() - 2), radius, radius);
        painter.setPen(tokens.color("textMuted"));
        painter.drawText(QRect(QPoint(0, 0), size), Qt::AlignCenter | Qt::TextWordWrap,
                         path.isEmpty() ? tr("No picture chosen") : tr("This picture can not be read"));
        m_ui->wallpaperPreview->setPixmap(pixmap);
        return;
    }
    wallpaper.setBlurRadius(m_ui->wallpaperBlurSlider->value());
    wallpaper.setDim(m_ui->wallpaperDimSlider->value(), tokens.color("surface"));
    wallpaper.prepare(size, ratio);
    wallpaper.paintBackground(&painter, QRectF(QPointF(0, 0), QSizeF(size)), radius);

    // three tiles, the first one selected, with the icons of real instances
    const QStringList icons{ "grass", "creeper", "skeleton" };
    const int tile = 72;
    for (int i = 0; i < 3; i++) {
        const QRect card(18 + i * (tile + 10), 20, tile, tile + 14);
        wallpaper.paintTile(&painter, card, std::min(radius, 12.0), i == 0, false);
        const QIcon icon = APPLICATION->icons()->getIcon(icons[i]);
        icon.paint(&painter, QRect(card.center().x() - 18, card.top() + 12, 36, 36));
        painter.setPen(tokens.color("text"));
        QFont font = painter.font();
        font.setPointSizeF(font.pointSizeF() * 0.85);
        painter.setFont(font);
        painter.drawText(QRect(card.left(), card.top() + 54, card.width(), 26), Qt::AlignHCenter | Qt::AlignTop, tr("Instance"));
        painter.setFont(this->font());
    }
    m_ui->wallpaperPreview->setPixmap(pixmap);
}

void AppearanceWidget::fitThemeCards()
{
    auto* cards = m_ui->themeCards;
    // the viewport catches up with a resize only after this runs, the list itself is already there
    const int perRow = std::max(1, cards->contentsRect().width() / cards->gridSize().width());
    const int rows = std::max(1, static_cast<int>(std::ceil(cards->count() / static_cast<double>(perRow))));
    cards->setFixedHeight(rows * cards->gridSize().height() + 2 * cards->frameWidth() + 4);
}

bool AppearanceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_ui->themeCards && event->type() == QEvent::Resize) {
        fitThemeCards();
    }
    // the cards never scroll, the page does
    if (watched == m_ui->themeCards->viewport() && event->type() == QEvent::Wheel) {
        QCoreApplication::sendEvent(m_ui->scrollArea->verticalScrollBar(), event);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
