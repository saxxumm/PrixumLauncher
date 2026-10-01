// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ExternalResourcesPage.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_ExternalResourcesPage.h"

#include "DesktopServices.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "ui/GuiUtil.h"
#include "ui/themes/NovaIcons.h"
#include "ui/widgets/PageActionBar.h"

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <algorithm>
#include <functional>

namespace {
class LockDelegate : public QStyledItemDelegate {
   public:
    explicit LockDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const override
    {
        QStyleOptionViewItem option(opt);
        initStyleOption(&option, index);

        bool locked = index.data(Qt::UserRole).toBool();

        option.text.clear();
        option.icon = QIcon::fromTheme(locked ? "lock" : "unlock");
        option.features |= QStyleOptionViewItem::HasDecoration;
        option.decorationAlignment = Qt::AlignBottom | Qt::AlignHCenter;
        option.decorationPosition = QStyleOptionViewItem::Top;

        int size = qMin(option.rect.width(), option.rect.height()) * 3 / 4;
        option.decorationSize = QSize(size, size);

        option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& /*option*/, const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            bool locked = index.data(Qt::UserRole).toBool();
            model->setData(index, !locked, Qt::UserRole);
            return true;
        }
        return event->type() == QEvent::MouseButtonDblClick;  // if double click ignore it
    }
};

bool isDisabled(const QModelIndex& index)
{
    // every resource model has its enable checkbox in the first column
    return index.siblingAtColumn(0).data(Qt::CheckStateRole) == Qt::Unchecked;
}

/// keeps a resource's icon in its own colors while the row is selected, grays it out while the resource is disabled
class IconDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

   protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (option->icon.isNull()) {
            return;
        }
        QPixmap pixmap = option->icon.pixmap(option->decorationSize);
        if (isDisabled(index)) {
            const QStyle* style = option->widget ? option->widget->style() : QApplication::style();
            pixmap = style->generatedIconPixmap(QIcon::Disabled, pixmap, option);
        }
        QIcon icon(pixmap);
        icon.addPixmap(pixmap, QIcon::Selected);
        option->icon = icon;
    }
};

/// the name in the regular text color with a muted second line under it
class NameDelegate : public QStyledItemDelegate {
   public:
    using SecondaryText = std::function<QString(const QModelIndex&)>;

    NameDelegate(SecondaryText secondary, QObject* parent) : QStyledItemDelegate(parent), m_secondary(std::move(secondary)) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const override
    {
        QStyleOptionViewItem option(opt);
        initStyleOption(&option, index);
        const QString secondary = m_secondary(index);
        if (secondary.isEmpty()) {
            QStyledItemDelegate::paint(painter, opt, index);
            return;
        }

        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, widget);
        const QString title = option.text;
        option.text.clear();
        // background, hover and selection
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, widget);

        QFont titleFont = option.font;
        titleFont.setWeight(QFont::DemiBold);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics metrics(option.font);
        int y = textRect.top() + (textRect.height() - titleMetrics.height() - metrics.height()) / 2;

        const auto group = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
        const auto titleRole = isDisabled(index)                         ? QPalette::PlaceholderText
                               : (option.state & QStyle::State_Selected) ? QPalette::HighlightedText
                                                                         : QPalette::Text;
        painter->save();
        painter->setFont(titleFont);
        painter->setPen(option.palette.color(group, titleRole));
        painter->drawText(QRect(textRect.left(), y, textRect.width(), titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(title, Qt::ElideRight, textRect.width()));
        y += titleMetrics.height();
        painter->setFont(option.font);
        painter->setPen(option.palette.color(group, QPalette::PlaceholderText));
        painter->drawText(QRect(textRect.left(), y, textRect.width(), metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(secondary, Qt::ElideRight, textRect.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), QFontMetrics(option.font).height() * 2 + 14));
        return size;
    }

   private:
    SecondaryText m_secondary;
};
}  // namespace

ExternalResourcesPage::ExternalResourcesPage(MinecraftInstance* instance, ResourceFolderModel* model, QWidget* parent)
    : QMainWindow(parent)
    , m_instance(instance)
    , m_ui(new Ui::ExternalResourcesPage)
    , m_model(model)
    , m_filterModel(static_cast<ResourceFolderModel::ProxyModel*>(ResourceFolderModel::createFilterProxyModel(this)))
{
    m_ui->setupUi(this);
    setupActionBar();

    m_ui->actionsToolbar->insertSpacer(m_ui->actionViewFolder);

    m_filterModel->setDynamicSortFilter(true);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterKeyColumn(-1);
    m_ui->treeView->setModel(m_filterModel);

    // keep the Update at the end of the list(otherwise there will be a need to iterate over the columns)
    auto lockColumn = static_cast<int>(model->columnNames(false).size()) - 1;
    m_ui->treeView->setItemDelegateForColumn(lockColumn, new LockDelegate(m_ui->treeView));
    // must come after setModel
    m_ui->treeView->setResizeModes(m_model->columnResizeModes());
    const auto columns = model->columnNames(false);
    for (const auto* narrow : { "Enable", "Image" }) {
        if (auto column = columns.indexOf(narrow); column >= 0) {
            m_ui->treeView->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        }
    }
    if (auto imageColumn = columns.indexOf("Image"); imageColumn >= 0) {
        m_ui->treeView->setItemDelegateForColumn(imageColumn, new IconDelegate(m_ui->treeView));
    }
    if (auto nameColumn = columns.indexOf("Name"); nameColumn >= 0) {
        auto secondLine = [this](const QModelIndex& index) {
            const auto source = m_filterModel->mapToSource(index);
            if (!source.isValid()) {
                return QString();
            }
            const auto& resource = m_model->at(source.row());
            const auto text = secondaryText(resource);
            if (resource.enabled()) {
                return text;
            }
            return text.isEmpty() ? tr("disabled") : tr("%1 · disabled").arg(text);
        };
        m_ui->treeView->setItemDelegateForColumn(nameColumn, new NameDelegate(secondLine, m_ui->treeView));
    }

    m_ui->treeView->installEventFilter(this);
    m_ui->treeView->sortByColumn(std::max(0, static_cast<int>(columns.indexOf("Name"))), Qt::AscendingOrder);
    m_ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // The default function names by Qt are pretty ugly, so let's just connect the actions manually,
    // to make it easier to read :)
    connect(m_ui->actionAddItem, &QAction::triggered, this, &ExternalResourcesPage::addItem);
    connect(m_ui->actionRemoveItem, &QAction::triggered, this, &ExternalResourcesPage::removeItem);
    connect(m_ui->actionEnableItem, &QAction::triggered, this, &ExternalResourcesPage::enableItem);
    connect(m_ui->actionDisableItem, &QAction::triggered, this, &ExternalResourcesPage::disableItem);
    connect(m_ui->actionViewHomepage, &QAction::triggered, this, &ExternalResourcesPage::viewHomepage);
    connect(m_ui->actionViewConfigs, &QAction::triggered, this, &ExternalResourcesPage::viewConfigs);
    connect(m_ui->actionViewFolder, &QAction::triggered, this, &ExternalResourcesPage::viewFolder);

    connect(m_ui->treeView, &ModListView::customContextMenuRequested, this, &ExternalResourcesPage::showContextMenu);
    connect(m_ui->treeView, &ModListView::activated, this, &ExternalResourcesPage::itemActivated);

    connect(m_ui->actionLockUpdates, &QAction::triggered, this, &ExternalResourcesPage::lockUpdates);
    connect(m_ui->actionUnlockUpdates, &QAction::triggered, this, &ExternalResourcesPage::unlockUpdates);

    auto* selectionModel = m_ui->treeView->selectionModel();

    connect(selectionModel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex& previous) {
        if (!current.isValid()) {
            m_ui->frame->clear();
            return;
        }

        updateFrame(current, previous);
    });

    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });
    for (auto signal : { &ResourceFolderModel::rowsInserted, &ResourceFolderModel::rowsRemoved }) {
        connect(m_model, signal, this, [this] {
            updateActions();
            updateCounts();
        });
    }
    connect(m_model, &ResourceFolderModel::dataChanged, this, [this] {
        updateActions();
        updateCounts();
    });
    connect(m_model, &ResourceFolderModel::modelReset, this, &ExternalResourcesPage::updateCounts);
    connect(m_model, &ResourceFolderModel::updateFinished, this, &ExternalResourcesPage::updateCounts);

    auto* viewHeader = m_ui->treeView->header();
    viewHeader->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(viewHeader, &QHeaderView::customContextMenuRequested, this, &ExternalResourcesPage::showHeaderContextMenu);

    m_model->loadColumns(m_ui->treeView);
    connect(m_ui->treeView->header(), &QHeaderView::sectionResized, this, [this] { m_model->saveColumns(m_ui->treeView); });
    connect(m_ui->filterEdit, &QLineEdit::textChanged, this, &ExternalResourcesPage::filterTextChanged);
    updateActions();
}

ExternalResourcesPage::~ExternalResourcesPage()
{
    delete m_ui;
}

QMenu* ExternalResourcesPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(m_ui->actionsToolbar->toggleViewAction());
    return filteredMenu;
}

void ExternalResourcesPage::showContextMenu(const QPoint& pos)
{
    auto* menu = m_ui->actionsToolbar->createContextMenu(this, tr("Context menu"));
    menu->exec(m_ui->treeView->mapToGlobal(pos));
    delete menu;
}

void ExternalResourcesPage::showHeaderContextMenu(const QPoint& pos)
{
    auto* menu = m_model->createHeaderContextMenu(m_ui->treeView);
    menu->exec(m_ui->treeView->mapToGlobal(pos));
    menu->deleteLater();
}

void ExternalResourcesPage::setupActionBar()
{
    using NovaIcons::Tint;
    // the toolbar only feeds the context menu now, the page has its own buttons
    m_ui->actionsToolbar->hide();

    m_ui->actionDownloadItem->setIcon(NovaIcons::icon("download", Tint::AccentText));
    m_ui->actionUpdateItem->setIcon(NovaIcons::icon("update"));
    m_ui->actionAddItem->setIcon(NovaIcons::icon("plus"));
    m_ui->actionEnableItem->setIcon(NovaIcons::icon("check-circle", Tint::Success));
    m_ui->actionDisableItem->setIcon(NovaIcons::icon("x-circle", Tint::Muted));
    m_ui->actionRemoveItem->setIcon(NovaIcons::icon("trash", Tint::Danger));
    m_ui->actionChangeVersion->setIcon(NovaIcons::icon("history"));
    m_ui->actionLockUpdates->setIcon(NovaIcons::icon("lock"));
    m_ui->actionUnlockUpdates->setIcon(NovaIcons::icon("unlock"));
    m_ui->actionViewHomepage->setIcon(NovaIcons::icon("globe"));
    m_ui->actionExportMetadata->setIcon(NovaIcons::icon("export"));
    m_ui->actionViewFolder->setIcon(NovaIcons::icon("folder"));
    m_ui->actionViewConfigs->setIcon(NovaIcons::icon("settings"));

    m_ui->downloadButton->setDefaultAction(m_ui->actionDownloadItem);
    m_ui->updateButton->setDefaultAction(m_ui->actionUpdateItem);
    m_ui->addFileButton->setDefaultAction(m_ui->actionAddItem);
    m_ui->enableButton->setDefaultAction(m_ui->actionEnableItem);
    m_ui->disableButton->setDefaultAction(m_ui->actionDisableItem);
    m_ui->removeButton->setDefaultAction(m_ui->actionRemoveItem);

    m_ui->moreButton->setIcon(NovaIcons::icon("more"));
    auto* moreMenu = new QMenu(m_ui->moreButton);
    connect(moreMenu, &QMenu::aboutToShow, this, [this, moreMenu] {
        // whatever the page offers besides its buttons
        PageActionBar::fillMenu(moreMenu, m_ui->actionsToolbar,
                                { m_ui->actionDownloadItem, m_ui->actionUpdateItem, m_ui->actionAddItem, m_ui->actionEnableItem,
                                  m_ui->actionDisableItem, m_ui->actionRemoveItem });
    });
    m_ui->moreButton->setMenu(moreMenu);

    m_ui->filterEdit->addAction(NovaIcons::icon("search", Tint::Muted), QLineEdit::LeadingPosition);

    using StateFilter = ResourceFolderModel::ProxyModel::StateFilter;
    for (auto [button, filter] : { std::pair{ m_ui->allFilter, StateFilter::All }, std::pair{ m_ui->enabledFilter, StateFilter::Enabled },
                                   std::pair{ m_ui->disabledFilter, StateFilter::Disabled } }) {
        connect(button, &QPushButton::toggled, this, [this, filter](bool checked) {
            if (checked) {
                m_filterModel->setStateFilter(filter);
            }
        });
    }
}

void ExternalResourcesPage::updateCounts()
{
    const auto all = m_model->allResources();
    const auto enabled = std::ranges::count_if(all, [](Resource* resource) { return resource->enabled(); });
    m_ui->allFilter->setText(tr("All (%1)").arg(all.size()));
    m_ui->enabledFilter->setText(tr("Enabled (%1)").arg(enabled));
    m_ui->disabledFilter->setText(tr("Disabled (%1)").arg(all.size() - enabled));
    updatePlaceholder();
}

void ExternalResourcesPage::updatePlaceholder()
{
    if (m_model->empty()) {
        m_ui->treeView->setPlaceholder(
            icon(), tr("Nothing here yet"),
            tr("Press \"%1\" to find something on Modrinth and CurseForge, or drop files here.").arg(m_ui->actionDownloadItem->iconText()));
    } else {
        m_ui->treeView->setPlaceholder(NovaIcons::icon("search", NovaIcons::Tint::Muted), tr("Nothing found"),
                                       tr("Try another search or filter."));
    }
}

QString ExternalResourcesPage::secondaryText(const Resource& resource) const
{
    // the file name only tells something new when the resource has a name of its own
    const auto fileName = resource.fileinfo().fileName();
    return fileName.startsWith(resource.name()) ? QString() : fileName;
}

QString ExternalResourcesPage::plainLine(QString text)
{
    static const QRegularExpression s_formatting("\u00A7.");
    return text.remove(s_formatting).simplified();
}

void ExternalResourcesPage::openedImpl()
{
    m_model->startWatching();
    updateCounts();
}

void ExternalResourcesPage::closedImpl()
{
    m_model->stopWatching();
}

void ExternalResourcesPage::retranslate()
{
    m_ui->retranslateUi(this);
    updateCounts();
}

void ExternalResourcesPage::itemActivated(const QModelIndex& /*unused*/)
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::TOGGLE);
}

void ExternalResourcesPage::filterTextChanged(const QString& newContents)
{
    m_viewFilter = newContents;
    m_filterModel->setFilterRegularExpression(m_viewFilter);
}

bool ExternalResourcesPage::shouldDisplay() const
{
    return true;
}

bool ExternalResourcesPage::listFilter(QKeyEvent* keyEvent)
{
    switch (keyEvent->key()) {
        case Qt::Key_Delete:
            removeItem();
            return true;
        case Qt::Key_Plus:
            addItem();
            return true;
        default:
            break;
    }
    return QWidget::eventFilter(m_ui->treeView, keyEvent);
}

bool ExternalResourcesPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(obj, ev);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(ev);
    if (obj == m_ui->treeView) {
        return listFilter(keyEvent);
    }

    return QWidget::eventFilter(obj, ev);
}

void ExternalResourcesPage::addItem()
{
    auto list = GuiUtil::browseForFiles(
        helpPage(), tr("Select %1", "Select whatever type of files the page contains. Example: 'Loader Mods'").arg(displayName()),
        m_fileSelectionFilter.arg(displayName()), APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty()) {
        for (const auto& filename : list) {
            m_model->installResource(filename);
        }
    }
}

void ExternalResourcesPage::removeItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());

    int count = 0;
    bool folder = false;
    for (auto& i : selection.indexes()) {
        if (i.column() == 0) {
            count++;

            // if a folder is selected, show the confirmation dialog
            if (m_model->at(i.row()).fileinfo().isDir()) {
                folder = true;
            }
        }
    }

    QString text;
    bool multiple = count > 1;

    if (multiple) {
        text = tr("You are about to remove %1 items.\n"
                  "This may be permanent and they will be gone from the folder.\n\n"
                  "Are you sure?")
                   .arg(count);
    } else if (folder) {
        text = tr("You are about to remove the folder \"%1\".\n"
                  "This may be permanent and it will be gone from the parent folder.\n\n"
                  "Are you sure?")
                   .arg(m_model->at(selection.indexes().at(0).row()).fileinfo().fileName());
    }

    if (!text.isEmpty()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"), text, QMessageBox::Warning,
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }

    removeItems(selection);
}

void ExternalResourcesPage::removeItems(const QItemSelection& selection)
{
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Delete"),
                                                     tr("If you remove this resource while the game is running it may crash your game.\n"
                                                        "Are you sure you want to do this?"),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }
    m_model->deleteResources(selection.indexes());
}

void ExternalResourcesPage::enableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::ENABLE);
}

void ExternalResourcesPage::disableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::DISABLE);
}

void ExternalResourcesPage::viewHomepage()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
    for (auto* resource : m_model->selectedResources(selection)) {
        auto url = resource->homepage();
        if (!url.isEmpty()) {
            DesktopServices::openUrl(url);
        }
    }
}

void ExternalResourcesPage::viewConfigs()
{
    DesktopServices::openPath(m_instance->instanceConfigFolder(), true);
}

void ExternalResourcesPage::viewFolder()
{
    DesktopServices::openPath(m_model->dir().absolutePath(), true);
}

void ExternalResourcesPage::updateActions()
{
    const bool hasSelection = m_ui->treeView->selectionModel()->hasSelection();
    const QModelIndexList selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
    const QList<Resource*> selectedResources = m_model->selectedResources(selection);
    const bool hasUpdatesUnlocked = hasSelection && std::ranges::any_of(selectedResources, [](Resource* resource) {
                                        return resource->metadata() && !resource->lockUpdate();
                                    });
    const bool hasUpdatesLocked = hasSelection && std::ranges::any_of(selectedResources, [](Resource* resource) {
                                      return resource->metadata() && resource->lockUpdate();
                                  });
    const bool allSelectedUpdatesLocked =
        hasSelection && std::ranges::all_of(selectedResources, [](Resource* resource) { return resource->lockUpdate(); });

    m_ui->actionUpdateItem->setEnabled(!m_model->empty() && !allSelectedUpdatesLocked);
    m_ui->actionResetItemMetadata->setEnabled(hasSelection);

    m_ui->actionChangeVersion->setEnabled(selectedResources.size() == 1 && selectedResources[0]->metadata() != nullptr);

    m_ui->actionRemoveItem->setEnabled(hasSelection);
    m_ui->actionEnableItem->setEnabled(hasSelection);
    m_ui->actionDisableItem->setEnabled(hasSelection);

    m_ui->actionViewHomepage->setEnabled(hasSelection && std::any_of(selectedResources.begin(), selectedResources.end(),
                                                                     [](Resource* resource) { return !resource->homepage().isEmpty(); }));

    m_ui->actionLockUpdates->setEnabled(hasUpdatesUnlocked);
    m_ui->actionUnlockUpdates->setEnabled(hasUpdatesLocked);
    m_ui->actionExportMetadata->setEnabled(!m_model->empty());

    // the bulk actions only show up next to the filters while something is selected
    m_ui->selectionLabel->setText(tr("Selected: %1").arg(selectedResources.size()));
    for (auto* widget :
         std::initializer_list<QWidget*>{ m_ui->selectionLabel, m_ui->enableButton, m_ui->disableButton, m_ui->removeButton }) {
        widget->setVisible(hasSelection);
    }
}

void ExternalResourcesPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    auto sourceCurrent = m_filterModel->mapToSource(current);
    int row = sourceCurrent.row();
    const Resource& resource = m_model->at(row);
    m_ui->frame->updateWithResource(resource);
}

void ExternalResourcesPage::lockUpdates()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setUpdateLock(selection.indexes(), EnableAction::ENABLE);
    updateActions();
}

void ExternalResourcesPage::unlockUpdates()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setUpdateLock(selection.indexes(), EnableAction::DISABLE);
    updateActions();
}