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

#include <QImage>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QPushButton;
class SettingsObject;

/// Split view showing the same scene at native resolution and after render scaling
class RenderScalePreview : public QWidget {
    Q_OBJECT
   public:
    explicit RenderScalePreview(QWidget* parent = nullptr);

    void setScale(int percent, const QString& filter, int sharpness);

    QSize sizeHint() const override { return { 520, 220 }; }
    QSize minimumSizeHint() const override { return { 240, 140 }; }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

   private:
    void regenerate();

    int m_percent = 50;
    QString m_filter = "nearest";
    int m_sharpness = 2;
    double m_split = 0.5;
    QImage m_native;
    QImage m_scaled;
};

class RenderScalingWidget : public QWidget {
    Q_OBJECT
   public:
    /// @param instanceMode shows the "override" group like the other per-instance settings
    explicit RenderScalingWidget(bool instanceMode, QWidget* parent = nullptr);

    void loadSettings(SettingsObject* settings);
    void saveSettings(SettingsObject* settings);

    /// size of the game window as configured on the "General" tab, used to show the resulting resolution
    void setWindowSize(const QSize& size, bool maximized);

   private:
    void refreshGamescopeStatus();
    void updateState();

    bool m_instanceMode;
    QSize m_windowSize{ 854, 480 };
    bool m_maximized = false;

    QGroupBox* m_group = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_recheck = nullptr;
    QCheckBox* m_enabled = nullptr;
    QWidget* m_options = nullptr;
    QSlider* m_percentSlider = nullptr;
    QSpinBox* m_percentSpin = nullptr;
    QLabel* m_resolution = nullptr;
    QComboBox* m_filter = nullptr;
    QLabel* m_sharpnessLabel = nullptr;
    QSlider* m_sharpness = nullptr;
    QComboBox* m_scaler = nullptr;
    QCheckBox* m_fullscreen = nullptr;
    QCheckBox* m_grabCursor = nullptr;
    QLineEdit* m_extraArgs = nullptr;
    RenderScalePreview* m_preview = nullptr;
};
