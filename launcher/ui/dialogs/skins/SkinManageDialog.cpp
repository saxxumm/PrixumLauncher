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

#include "SkinManageDialog.h"
#include "ui_SkinManageDialog.h"

#include <FileSystem.h>
#include <QAction>
#include <QClipboard>
#include <QDialog>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QListView>
#include <QMenu>
#include <QMimeDatabase>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QUrl>

#include "Application.h"
#include "DesktopServices.h"
#include "QObjectPtr.h"
#include "settings/SettingsObject.h"

#include "minecraft/auth/Parsers.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/SkinRequests.h"

#include "net/NetJob.h"
#include "net/Request.h"
#include "tasks/Task.h"

#include "Json.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/skins/SkinPreviewWidget.h"
#include "ui/themes/NovaIcons.h"

SkinManageDialog::SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct)
    : QDialog(parent)
    , m_acct(acct)
    , m_ui(new Ui::SkinManageDialog)
    , m_list(this, APPLICATION->settings()->get("SkinsDir").toString(), acct)
    , m_grid(&m_list, this)
{
    m_ui->setupUi(this);
    setWindowModality(Qt::WindowModal);
    if (auto* screen = parent ? parent->screen() : QGuiApplication::primaryScreen()) {
        resize(QSize(1180, 760).boundedTo(screen->availableSize() * 0.9));
    }

    m_preview = new SkinPreviewWidget(this);
    m_ui->skinLayout->insertWidget(2, m_preview, 1);
    m_ui->nameTag->setText(m_acct->profileName());
    m_ui->nameTag->setObjectName("skinNameTag");
    m_ui->skinsTitle->setObjectName("skinsTitle");
    m_ui->savedTitle->setObjectName("skinsTitle");
    m_ui->clipboardNotice->setObjectName("skinClipboardNotice");
    m_ui->clipboardNotice->hide();
    m_ui->clipboardLayout->setStretch(0, 1);
    m_ui->clipboardCloseBtn->setIcon(NovaIcons::icon("close", NovaIcons::Tint::Muted));
    m_ui->importBtn->setIcon(NovaIcons::icon("plus"));
    m_ui->openDirBtn->setIcon(NovaIcons::icon("folder"));

    auto* view = m_ui->listView;
    view->setObjectName("skinGrid");
    m_cards = new SkinCardDelegate(view);
    view->setItemDelegate(m_cards);
    view->setModel(&m_grid);
    view->setGridSize(QSize(168, 228));
    view->setMouseTracking(true);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(false);
    view->viewport()->setAcceptDrops(true);
    view->setDragDropMode(QAbstractItemView::DropOnly);
    view->setDefaultDropAction(Qt::CopyAction);
    view->installEventFilter(this);

    connect(view, &QAbstractItemView::doubleClicked, this, &SkinManageDialog::activated);
    // a single click on the card in front opens the file picker
    connect(view, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
        if (index.data(SkinGridModel::AddCardRole).toBool()) {
            addFromFile();
        }
    });
    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SkinManageDialog::selectionChanged);
    connect(view, &QListView::customContextMenuRequested, this, &SkinManageDialog::show_context_menu);
    connect(&m_list, &SkinList::remoteUrlsDropped, this, [this](const QList<QUrl>& urls) {
        for (const auto& url : urls) {
            importSource(SkinSource::parse(url.toString()));
        }
    });

    connect(m_ui->importBtn, &QPushButton::clicked, this, &SkinManageDialog::importFromLine);
    // Enter in the field adds the skin and nothing else, it must not reach a default button that wears a skin
    m_ui->urlLine->installEventFilter(this);
    connect(m_ui->clipboardAddBtn, &QPushButton::clicked, this, [this] {
        m_ui->clipboardNotice->hide();
        m_dismissedClipboard = m_clipboardSource.url.toString();
        importSource(m_clipboardSource);
    });
    connect(m_ui->clipboardCloseBtn, &QToolButton::clicked, this, [this] {
        m_dismissedClipboard = m_clipboardSource.url.toString();
        m_ui->clipboardNotice->hide();
    });
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &SkinManageDialog::checkClipboard);

    // the players on the cards keep walking
    m_animation.setInterval(40);
    connect(&m_animation, &QTimer::timeout, this, [this] {
        m_cards->setTime(m_clock.elapsed() / 1000.0);
        m_ui->listView->viewport()->update();
    });
    m_clock.start();

    setupCapes();

    view->setCurrentIndex(m_grid.cardOf(m_list.getSelectedAccountSkin()));

    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Wear Skin"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setProperty("novaRole", "accent");
    // wearing a skin uploads it, that takes a click and never just the Enter key
    for (auto* button : findChildren<QPushButton*>()) {
        button->setAutoDefault(false);
        button->setDefault(false);
    }
    checkClipboard();
}

SkinManageDialog::~SkinManageDialog()
{
    delete m_ui;
}

void SkinManageDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    m_animation.start();
}

void SkinManageDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    m_animation.stop();
}

void SkinManageDialog::changeEvent(QEvent* event)
{
    // coming back from the browser with a copied link
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        checkClipboard();
    }
    QDialog::changeEvent(event);
}

void SkinManageDialog::keyPressEvent(QKeyEvent* event)
{
    // QDialogButtonBox makes "Wear Skin" the default button again whenever the dialog shows, so Enter in the cape list
    // or the name field would upload the skin; only a click or a double click on a skin does that
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void SkinManageDialog::activated(QModelIndex index)
{
    if (index.data(SkinGridModel::AddCardRole).toBool()) {
        return;
    }
    m_selectedSkinKey = index.data(Qt::UserRole).toString();
    accept();
}

void SkinManageDialog::selectionChanged(const QItemSelection& selected, [[maybe_unused]] const QItemSelection& deselected)
{
    if (selected.empty()) {
        return;
    }

    QString key = selected.first().indexes().first().data(Qt::UserRole).toString();
    if (key.isEmpty()) {
        return;
    }
    m_selectedSkinKey = key;
    auto* skin = getSelectedSkin();
    if (!skin) {
        return;
    }

    updatePreview();
    m_ui->capeCombo->setCurrentIndex(m_capesIdx.value(skin->getCapeId()));
    m_ui->steveBtn->setChecked(skin->getModel() == SkinModel::CLASSIC);
    m_ui->alexBtn->setChecked(skin->getModel() == SkinModel::SLIM);
}

void SkinManageDialog::updatePreview()
{
    auto* skin = getSelectedSkin();
    if (!skin) {
        return;
    }
    m_preview->setSkin(skin->getTexture(), skin->getModel() == SkinModel::SLIM);
    m_preview->setCape(m_capes.value(skin->getCapeId()));
}

void SkinManageDialog::on_openDirBtn_clicked()
{
    DesktopServices::openPath(m_list.getDir(), true);
}

void SkinManageDialog::addFromFile()
{
    auto filter = QMimeDatabase().mimeTypeForName("image/png").filterString();
    QString rawPath = QFileDialog::getOpenFileName(this, tr("Select Skin Texture"), QString(), filter);
    if (rawPath.isNull()) {
        return;
    }
    auto message = m_list.installSkin(rawPath, {});
    if (!message.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Selected file is not a valid skin"), message, QMessageBox::Critical)->show();
        return;
    }
    selectWhenListed(QFileInfo(rawPath).completeBaseName());
}

void SkinManageDialog::importFromLine()
{
    const auto source = SkinSource::parse(m_ui->urlLine->text());
    if (source.kind == SkinSource::Source::Kind::None) {
        CustomMessageBox::selectable(this, tr("Nothing to add"), tr("Enter a player name or a link to a skin, for example from NameMC."),
                                     QMessageBox::Warning)
            ->show();
        return;
    }
    importSource(source);
}

void SkinManageDialog::importSource(const SkinSource::Source& source)
{
    if (m_importing) {
        return;
    }
    m_importing = true;
    bool added = false;
    switch (source.kind) {
        case SkinSource::Source::Kind::Url:
            added = importUrl(source.url, source.fileName);
            break;
        case SkinSource::Source::Kind::Player:
            added = importPlayer(source.player);
            break;
        case SkinSource::Source::Kind::None:
            break;
    }
    m_importing = false;
    if (added) {
        m_ui->urlLine->clear();
    }
}

bool SkinManageDialog::importUrl(const QUrl& url, const QString& fileName)
{
    const auto path = FS::PathCombine(m_list.getDir(), fileName);
    const auto key = QFileInfo(path).completeBaseName();
    // the same NameMC skin twice is the same file
    if (QFileInfo::exists(path) && m_list.skin(key)) {
        selectWhenListed(key);
        return true;
    }

    NetJob::Ptr job{ new NetJob(tr("Download skin"), APPLICATION->network()) };
    job->setAskRetry(false);
    job->addNetAction(Net::Request::makeFile(url, path));
    ProgressDialog dlg(this);
    dlg.execWithTask(job.get());
    SkinModel s(path);
    if (!s.isValid()) {
        CustomMessageBox::selectable(this, tr("URL is not a valid skin"),
                                     QFileInfo::exists(path) ? tr("Skin images must be 64x64 or 64x32 pixel PNG files.")
                                                             : tr("Unable to download the skin: '%1'.").arg(url.toString()),
                                     QMessageBox::Critical)
            ->show();
        QFile::remove(path);
        return false;
    }
    selectWhenListed(key);
    return true;
}

void SkinManageDialog::selectWhenListed(const QString& key)
{
    auto select = [this, key] {
        const int row = m_list.getSkinIndex(key);
        if (row < 0) {
            return false;
        }
        m_ui->listView->setCurrentIndex(m_grid.cardOf(row));
        m_ui->listView->scrollTo(m_grid.cardOf(row));
        return true;
    };
    if (select()) {
        return;
    }
    auto* connection = new QMetaObject::Connection;
    auto once = [select, connection] {
        if (select()) {
            QObject::disconnect(*connection);
            delete connection;
        }
    };
    *connection = connect(&m_grid, &QAbstractItemModel::modelReset, this, once);
}

void SkinManageDialog::checkClipboard()
{
    const auto source = SkinSource::parse(QGuiApplication::clipboard()->text());
    const bool fresh = source.isSkinLink && source.url.toString() != m_dismissedClipboard &&
                       !QFileInfo::exists(FS::PathCombine(m_list.getDir(), source.fileName));
    if (!fresh) {
        m_ui->clipboardNotice->hide();
        return;
    }
    m_clipboardSource = source;
    m_ui->clipboardText->setText(source.url.host().contains("namemc") ? tr("There is a NameMC skin link in the clipboard.")
                                                                      : tr("There is a skin link in the clipboard."));
    m_ui->clipboardNotice->show();
}

namespace {
QPixmap previewCape(const QImage& capeImage)
{
    return QPixmap::fromImage(capeImage.copy(1, 1, 10, 16).scaled(20, 32, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}
}  // namespace

void SkinManageDialog::setupCapes()
{
    // FIXME: add a model for this, download/refresh the capes on demand
    auto& accountData = *m_acct->accountData();
    int index = 0;
    m_ui->capeCombo->addItem(tr("No Cape"), QVariant());
    auto currentCape = accountData.minecraftProfile.currentCape;
    if (currentCape.isEmpty()) {
        m_ui->capeCombo->setCurrentIndex(index);
    }

    auto capesDir = FS::PathCombine(m_list.getDir(), "capes");
    NetJob::Ptr job{ new NetJob(tr("Download capes"), APPLICATION->network()) };
    bool needsToDownload = false;
    for (auto& cape : accountData.minecraftProfile.capes) {
        auto path = FS::PathCombine(capesDir, cape.id + ".png");
        if (!cape.data.isEmpty()) {
            QImage capeImage;
            if (capeImage.loadFromData(cape.data, "PNG") && capeImage.save(path)) {
                m_capes[cape.id] = capeImage;
                continue;
            }
        }
        if (QFileInfo(path).exists()) {
            continue;
        }
        if (!cape.url.isEmpty()) {
            needsToDownload = true;
            job->addNetAction(Net::Request::makeFile(cape.url, path));
        }
    }
    if (needsToDownload) {
        ProgressDialog dlg(this);
        dlg.execWithTask(job.get());
    }
    for (auto& cape : accountData.minecraftProfile.capes) {
        index++;
        if (!m_capes.contains(cape.id)) {
            auto path = FS::PathCombine(capesDir, cape.id + ".png");
            if (QImage loaded; QFileInfo(path).exists() && loaded.load(path)) {
                m_capes[cape.id] = loaded;
            }
        }
        if (const QImage capeImage = m_capes.value(cape.id); !capeImage.isNull()) {
            m_ui->capeCombo->addItem(previewCape(capeImage), cape.alias, cape.id);
        } else {
            m_ui->capeCombo->addItem(cape.alias, cape.id);
        }

        m_capesIdx[cape.id] = index;
    }
}

void SkinManageDialog::on_capeCombo_currentIndexChanged(int /*index*/)
{
    auto id = m_ui->capeCombo->currentData();
    if (auto* skin = getSelectedSkin(); skin) {
        skin->setCapeId(id.toString());
    }
    m_preview->setCape(m_capes.value(id.toString(), {}));
}

void SkinManageDialog::on_steveBtn_toggled(bool checked)
{
    if (auto* skin = getSelectedSkin(); skin) {
        skin->setModel(checked ? SkinModel::CLASSIC : SkinModel::SLIM);
        updatePreview();
        m_ui->listView->viewport()->update();
    }
}

void SkinManageDialog::accept()
{
    auto* skin = m_list.skin(m_selectedSkinKey);
    if (!skin) {
        reject();
        return;
    }
    auto path = skin->getPath();

    ProgressDialog prog(this);
    NetJob::Ptr skinUpload{ new NetJob(tr("Change skin"), APPLICATION->network(), 1) };

    if (!QFile::exists(path)) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Skin file does not exist!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }

    skinUpload->addNetAction(makeSkinUploadRequest(m_acct->accessToken(), skin->getPath(), skin->getModelString()));

    auto selectedCape = skin->getCapeId();
    if (selectedCape != m_acct->accountData()->minecraftProfile.currentCape) {
        skinUpload->addNetAction(makeCapeChangeRequest(m_acct->accessToken(), selectedCape));
    }

    skinUpload->addTask(m_acct->refresh().staticCast<Task>());
    if (prog.execWithTask(skinUpload.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Failed to upload skin!"), QMessageBox::Warning)->exec();
        return;
    }
    skin->setURL(m_acct->accountData()->minecraftProfile.skin.url);
    QDialog::accept();
}

void SkinManageDialog::on_resetBtn_clicked()
{
    ProgressDialog prog(this);
    NetJob::Ptr skinReset{ new NetJob(tr("Reset skin"), APPLICATION->network(), 1) };
    skinReset->addNetAction(makeSkinDeleteRequest(m_acct->accessToken()));
    skinReset->addTask(m_acct->refresh().staticCast<Task>());
    if (prog.execWithTask(skinReset.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Delete"), tr("Failed to delete current skin!"), QMessageBox::Warning)->exec();
        return;
    }
    QDialog::accept();
}

void SkinManageDialog::show_context_menu(const QPoint& pos)
{
    if (m_ui->listView->indexAt(pos).data(SkinGridModel::AddCardRole).toBool() || !m_ui->listView->indexAt(pos).isValid()) {
        return;
    }
    QMenu myMenu(tr("Context menu"), this);
    myMenu.addAction(m_ui->action_Rename_Skin);
    myMenu.addAction(m_ui->action_Delete_Skin);

    myMenu.exec(m_ui->listView->mapToGlobal(pos));
}

bool SkinManageDialog::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_ui->urlLine && ev->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(ev)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            importFromLine();
            return true;
        }
    }
    if (obj == m_ui->listView) {
        if (ev->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                case Qt::Key_Delete:
                    on_action_Delete_Skin_triggered(false);
                    return true;
                case Qt::Key_F2:
                    on_action_Rename_Skin_triggered(false);
                    return true;
                default:
                    break;
            }
        }
    }
    return QDialog::eventFilter(obj, ev);
}

void SkinManageDialog::on_action_Rename_Skin_triggered(bool /*unused*/)
{
    if (!m_selectedSkinKey.isEmpty()) {
        m_ui->listView->edit(m_ui->listView->currentIndex());
    }
}

void SkinManageDialog::on_action_Delete_Skin_triggered(bool /*unused*/)
{
    if (m_selectedSkinKey.isEmpty()) {
        return;
    }

    if (m_list.getSkinIndex(m_selectedSkinKey) == m_list.getSelectedAccountSkin()) {
        CustomMessageBox::selectable(this, tr("Delete error"), tr("Can not delete skin that is in use."), QMessageBox::Warning)->exec();
        return;
    }

    auto* skin = m_list.skin(m_selectedSkinKey);
    if (!skin) {
        return;
    }

    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\".\n"
                                                    "Are you sure?")
                                                     .arg(skin->name()),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response == QMessageBox::Yes) {
        if (!m_list.deleteSkin(m_selectedSkinKey, true)) {
            m_list.deleteSkin(m_selectedSkinKey, false);
        }
    }
}

namespace {
class WaitTask : public Task {
   public:
    WaitTask() = default;
    ~WaitTask() override = default;

   public slots:
    void quit()
    {
        m_done = true;
        m_loop.quit();
    }

   protected:
    void executeTask() override
    {
        if (!m_done) {
            m_loop.exec();
        }
        emitSucceeded();
    };

   private:
    QEventLoop m_loop;
    bool m_done{};
};
}  // namespace

bool SkinManageDialog::importPlayer(const QString& user)
{
    MinecraftProfile mcProfile;
    auto path = FS::PathCombine(m_list.getDir(), user + ".png");

    NetJob::Ptr job{ new NetJob(tr("Download user skin"), APPLICATION->network(), 1) };
    job->setAskRetry(false);

    auto uuidLoop = makeShared<WaitTask>();
    auto profileLoop = makeShared<WaitTask>();

    auto [getUUID, uuidOut] = Net::Request::makeByteArray("https://api.minecraftservices.com/minecraft/profile/lookup/name/" + user);
    auto [getProfile, profileOut] = Net::Request::makeByteArray(QUrl());
    auto downloadSkin = Net::Request::makeFile(QUrl(), path);

    QString failReason;

    connect(getUUID.get(), &Task::aborted, uuidLoop.get(), &WaitTask::quit);
    connect(getUUID.get(), &Task::failed, this, [&failReason](const QString& reason) {
        qCritical() << "Couldn't get user UUID:" << reason;
        failReason = tr("failed to get user UUID");
    });
    connect(getUUID.get(), &Task::failed, uuidLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::aborted, profileLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::failed, profileLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::failed, this, [&failReason](const QString& reason) {
        qCritical() << "Couldn't get user profile:" << reason;
        failReason = tr("failed to get user profile");
    });
    connect(downloadSkin.get(), &Task::failed, this, [&failReason](const QString& reason) {
        qCritical() << "Couldn't download skin:" << reason;
        failReason = tr("failed to download skin");
    });

    connect(getUUID.get(), &Task::succeeded, this, [uuidLoop, uuidOut, job, getProfile, &failReason] {
        auto doc = Json::requireDocument(*uuidOut, "Minecraft skin service");
        if (!doc) {
            qWarning() << "Error while parsing JSON response from Minecraft skin service:" << doc.error();
            failReason = tr("failed to parse get user UUID response");
            uuidLoop->quit();
            return;
        }
        const auto root = doc->object();
        auto id = root["id"].toString();
        if (!id.isEmpty()) {
            getProfile->setUrl("https://sessionserver.mojang.com/session/minecraft/profile/" + id);
        } else {
            failReason = tr("user id is empty");
            job->abort();
        }
        uuidLoop->quit();
    });

    connect(getProfile.get(), &Task::succeeded, this, [profileLoop, profileOut, job, getProfile, &mcProfile, downloadSkin, &failReason] {
        if (Parsers::parseMinecraftProfileMojang(*profileOut, mcProfile)) {
            downloadSkin->setUrl(mcProfile.skin.url);
        } else {
            failReason = tr("failed to parse get user profile response");
            job->abort();
        }
        profileLoop->quit();
    });

    job->addNetAction(getUUID);
    job->addTask(uuidLoop);
    job->addNetAction(getProfile);
    job->addTask(profileLoop);
    job->addNetAction(downloadSkin);
    ProgressDialog dlg(this);
    dlg.execWithTask(job.get());

    SkinModel s(path);
    if (!s.isValid()) {
        if (failReason.isEmpty()) {
            failReason = tr("the skin is invalid");
        }
        CustomMessageBox::selectable(this, tr("Username not found"),
                                     tr("Unable to find the skin for '%1'\n because: %2.").arg(user, failReason), QMessageBox::Critical)
            ->show();
        QFile::remove(path);
        return false;
    }
    s.setModel(mcProfile.skin.variant.toUpper() == "SLIM" ? SkinModel::SLIM : SkinModel::CLASSIC);
    s.setURL(mcProfile.skin.url);
    if (m_capes.contains(mcProfile.currentCape)) {
        s.setCapeId(mcProfile.currentCape);
    }
    m_list.updateSkin(&s);
    selectWhenListed(s.name());
    return true;
}

SkinModel* SkinManageDialog::getSelectedSkin()
{
    if (auto* skin = m_list.skin(m_selectedSkinKey); skin && skin->isValid()) {
        return skin;
    }
    return nullptr;
}

QHash<QString, QImage> SkinManageDialog::capes()
{
    return m_capes;
}
