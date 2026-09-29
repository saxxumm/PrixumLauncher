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

#include <QDialog>
#include <QMap>
#include <QTimer>

#include "ui/themes/NovaTheme.h"

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QToolButton;

/// Visual editor for Nova themes. Every change is applied to the whole launcher right away.
class ThemeEditorDialog : public QDialog {
    Q_OBJECT
   public:
    explicit ThemeEditorDialog(QWidget* parent = nullptr);

    void reject() override;

   private:
    QWidget* createColorsPage();
    QWidget* createShapePage();
    QWidget* createStylesheetPage();
    QWidget* createPreview();

    void loadTheme(const QString& id);
    void setTokens(const Nova::Tokens& tokens);
    void setColor(const QString& key, const QColor& color);
    void scheduleApply();
    void applyNow();
    bool save(bool asNew);
    void exportTheme();

    Nova::Tokens m_tokens;
    /// theme that is being edited, empty or built-in means "save as new"
    QString m_themeId;
    bool m_loading = false;
    QTimer m_applyTimer;

    QLineEdit* m_name = nullptr;
    QComboBox* m_base = nullptr;
    QMap<QString, QToolButton*> m_swatches;
    QMap<QString, QLineEdit*> m_hexEdits;
    QMap<QString, QSpinBox*> m_metrics;
    QCheckBox* m_systemFont = nullptr;
    QFontComboBox* m_font = nullptr;
    QPlainTextEdit* m_qss = nullptr;
    QCheckBox* m_replaceBase = nullptr;
    QLabel* m_status = nullptr;
};
