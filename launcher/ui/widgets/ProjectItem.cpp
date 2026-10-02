#include "ProjectItem.h"

#include <QApplication>

#include <QIcon>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>

namespace {
constexpr int s_padding = 12;
constexpr int s_toggleHeight = 28;
}  // namespace

ProjectItemDelegate::ProjectItemDelegate(QWidget* parent) : QStyledItemDelegate(parent) {}

QString ProjectItemDelegate::downloadsText(qint64 downloads)
{
    const QLocale locale;
    QString count;
    if (downloads >= 1'000'000) {
        //: download count in millions, like "12.3M"
        count = tr("%1M").arg(locale.toString(downloads / 1'000'000.0, 'f', downloads >= 10'000'000 ? 0 : 1));
    } else if (downloads >= 1'000) {
        //: download count in thousands, like "450K"
        count = tr("%1K").arg(locale.toString(downloads / 1'000.0, 'f', downloads >= 10'000 ? 0 : 1));
    } else {
        count = locale.toString(downloads);
    }
    return QString::fromUtf8("↓ ") + count;
}

QString ProjectItemDelegate::toggleText(const QModelIndex& index, Qt::CheckState state) const
{
    if (state == Qt::Checked) {
        return QString::fromUtf8("✓ ") + tr("Added");
    }
    if (index.data(UserDataTypes::INSTALLED).toBool()) {
        return tr("Installed");
    }
    return "+ " + tr("Add");
}

QRect ProjectItemDelegate::toggleRect(const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    if (!(opt.features & QStyleOptionViewItem::HasCheckIndicator)) {
        return {};
    }
    // as wide as the longest of its texts, so it does not jump when clicked
    QFont font = opt.font;
    font.setBold(true);
    const QFontMetrics metrics(font);
    int width = 0;
    for (auto state : { Qt::Checked, Qt::Unchecked }) {
        width = std::max(width, metrics.horizontalAdvance(toggleText(index, state)));
    }
    width += 28;
    return { opt.rect.right() - s_padding - width, opt.rect.center().y() - s_toggleHeight / 2, width, s_toggleHeight };
}

void ProjectItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QStyle* style = opt.widget == nullptr ? QApplication::style() : opt.widget->style();
    const bool isChecked = opt.checkState == Qt::Checked;
    const bool isInstalled = index.data(UserDataTypes::INSTALLED).toBool();

    // background, hover and selection come from the style, the checkbox is replaced by our own button
    QStyleOptionViewItem background(opt);
    background.features &= ~QStyleOptionViewItem::HasCheckIndicator;
    background.text.clear();
    background.icon = QIcon();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &background, painter, opt.widget);

    painter->setRenderHint(QPainter::Antialiasing);
    const auto group = (opt.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    const QColor text = opt.palette.color(group, QPalette::Text);
    const QColor muted = opt.palette.color(group, QPalette::PlaceholderText);

    QRect content = opt.rect.adjusted(s_padding, 0, -s_padding, 0);

    // the pick button on the right
    if (const QRect toggle = toggleRect(opt, index); toggle.isValid()) {
        QFont font = opt.font;
        font.setBold(true);
        painter->setFont(font);
        const QColor accent = opt.palette.color(group, QPalette::Highlight);
        if (isChecked) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(accent);
        } else {
            painter->setPen(QPen(opt.palette.color(group, QPalette::Mid), 1));
            painter->setBrush(opt.palette.color(group, QPalette::Button));
        }
        painter->drawRoundedRect(QRectF(toggle).adjusted(0.5, 0.5, -0.5, -0.5), s_toggleHeight / 2.0, s_toggleHeight / 2.0);
        painter->setPen(isChecked ? opt.palette.color(group, QPalette::HighlightedText) : (isInstalled ? muted : text));
        painter->drawText(toggle, Qt::AlignCenter, toggleText(index, opt.checkState));
        content.setRight(toggle.left() - s_padding);
    }

    // the icon, rounded like the cards around it
    int iconSize = std::min(opt.decorationSize.height(), opt.rect.height() - 16);
    if (!opt.icon.isNull() && iconSize > 0) {
        const QRect iconRect(content.left(), opt.rect.center().y() - iconSize / 2, iconSize, iconSize);
        QPainterPath clip;
        clip.addRoundedRect(iconRect, 8, 8);
        painter->save();
        painter->setClipPath(clip);
        opt.icon.paint(painter, iconRect);
        painter->restore();
        content.setLeft(iconRect.right() + s_padding);
    }

    // title, with the authors and downloads in muted text after it
    QFont titleFont = opt.font;
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0) {
        titleFont.setPointSizeF(titleFont.pointSizeF() + 1);
    }
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics metrics(opt.font);

    QStringList meta;
    if (const auto author = index.data(UserDataTypes::AUTHOR).toString(); !author.isEmpty()) {
        meta << author;
    }
    if (const auto downloads = index.data(UserDataTypes::DOWNLOADS); downloads.isValid() && downloads.toLongLong() >= 0) {
        meta << downloadsText(downloads.toLongLong());
    }

    const QString description = index.data(UserDataTypes::DESCRIPTION).toString().simplified();
    const int descriptionLines =
        description.isEmpty() ? 0 : (opt.rect.height() >= titleMetrics.height() + metrics.height() * 2 + 14 ? 2 : 1);
    const int blockHeight = titleMetrics.height() + 2 + descriptionLines * metrics.height();
    int y = opt.rect.center().y() - blockHeight / 2;

    const QString title = index.data(UserDataTypes::TITLE).toString();
    const QString elidedTitle = titleMetrics.elidedText(title, Qt::ElideRight, content.width());
    painter->setFont(titleFont);
    painter->setPen(text);
    painter->drawText(QRect(content.left(), y, content.width(), titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);
    const int metaLeft = content.left() + titleMetrics.horizontalAdvance(elidedTitle) + 10;
    if (!meta.isEmpty() && metaLeft < content.right() - 40) {
        painter->setFont(opt.font);
        painter->setPen(muted);
        const QRect metaRect(metaLeft, y, content.right() - metaLeft, titleMetrics.height());
        painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(meta.join(" · "), Qt::ElideRight, metaRect.width()));
    }
    y += titleMetrics.height() + 2;

    // up to two lines of description, the last one elided
    if (descriptionLines > 0) {
        painter->setFont(opt.font);
        painter->setPen(muted);
        QTextLayout layout(description, opt.font);
        layout.beginLayout();
        int shown = 0;
        int start = 0;
        while (shown < descriptionLines) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) {
                break;
            }
            line.setLineWidth(content.width());
            start = line.textStart();
            QString lineText = description.mid(start, line.textLength());
            if (shown == descriptionLines - 1) {
                lineText = metrics.elidedText(description.mid(start), Qt::ElideRight, content.width());
            }
            painter->drawText(QRect(content.left(), y, content.width(), metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                              lineText.trimmed());
            y += metrics.height();
            shown++;
        }
        layout.endLayout();
    }

    painter->restore();
}

bool ProjectItemDelegate::editorEvent(QEvent* event,
                                      QAbstractItemModel* model,
                                      const QStyleOptionViewItem& option,
                                      const QModelIndex& index)
{
    if (!(event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonPress ||
          event->type() == QEvent::MouseButtonDblClick))
        return false;

    auto* mouseEvent = static_cast<QMouseEvent*>(event);

    if (mouseEvent->button() != Qt::LeftButton)
        return false;

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    if (!toggleRect(opt, index).contains(mouseEvent->position().toPoint()))
        return false;

    // swallow other events
    // (prevents item being selected or double click action triggering)
    if (event->type() != QEvent::MouseButtonRelease)
        return true;

    emit checkboxClicked(index);
    return true;
}
