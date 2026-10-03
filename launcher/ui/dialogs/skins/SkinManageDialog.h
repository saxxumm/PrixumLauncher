// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023-2024 Trial97 <alexandru.tripon97@gmail.com>
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
#include <QElapsedTimer>
#include <QItemSelection>
#include <QTimer>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/SkinSource.h"
#include "ui/dialogs/skins/SkinGrid.h"

class SkinPreviewWidget;

namespace Ui {
class SkinManageDialog;
}
class SkinManageDialog : public QDialog {
    Q_OBJECT
   public:
    explicit SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct);
    ~SkinManageDialog() override;

    SkinModel* getSelectedSkin();
    QHash<QString, QImage> capes();

   public slots:
    void selectionChanged(const QItemSelection&, const QItemSelection&);
    void activated(QModelIndex);
    void on_openDirBtn_clicked();
    void accept() override;
    void on_capeCombo_currentIndexChanged(int index);
    void on_steveBtn_toggled(bool checked);
    void on_resetBtn_clicked();
    void show_context_menu(const QPoint& pos);
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void on_action_Rename_Skin_triggered(bool checked);
    void on_action_Delete_Skin_triggered(bool checked);

   protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

   private:
    void setupCapes();
    void updatePreview();
    void addFromFile();
    void importFromLine();
    void importSource(const SkinSource::Source& source);
    bool importUrl(const QUrl& url, const QString& fileName);
    bool importPlayer(const QString& player);
    /// selects the skin as soon as the list has it, the folder watcher reports new files a moment later
    void selectWhenListed(const QString& key);
    /// offers a skin link from the clipboard, NameMC links mostly
    void checkClipboard();

   private:
    MinecraftAccountPtr m_acct;
    Ui::SkinManageDialog* m_ui;
    SkinList m_list;
    SkinGridModel m_grid;
    SkinCardDelegate* m_cards = nullptr;
    SkinPreviewWidget* m_preview = nullptr;
    QString m_selectedSkinKey;
    QHash<QString, QImage> m_capes;
    QHash<QString, int> m_capesIdx;
    QTimer m_animation;
    QElapsedTimer m_clock;
    SkinSource::Source m_clipboardSource;
    QString m_dismissedClipboard;
    /// downloads run in a nested event loop, a second one must not start meanwhile
    bool m_importing = false;
};
