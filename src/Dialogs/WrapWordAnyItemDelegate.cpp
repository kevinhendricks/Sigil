/************************************************************************
**
**  Copyright (C) 2025      Kevin B. Hendricks, Stratford Ontario Canada
**
**  This file is part of Sigil.
**
**  Sigil is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Sigil is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Sigil.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#include <QApplication>
#include <QDebug>
#include <QString>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTextDocument>
#include <QTextOption>

#include "Dialogs/WrapWordAnyItemDelegate.h"

WrapWordAnyItemDelegate::WrapWordAnyItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

WrapWordAnyItemDelegate::~WrapWordAnyItemDelegate()
{
}

void WrapWordAnyItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &inOption,
                                    const QModelIndex &index) const
{
    if (index.column() != 1) {
        QStyledItemDelegate::paint(painter, inOption, index);
        return;
    }
    // must first init option for this specific index
    QStyleOptionViewItem option = inOption;
    initStyleOption(&option, index);

    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setTextDirection(option.direction);
    QTextDocument doc;
    doc.setDefaultTextOption(textOption);
    doc.setPlainText(option.text);
    doc.setTextWidth(option.rect.width());
    doc.setDefaultFont(option.font);
    doc.setDocumentMargin(0);

    // Painting item without text (this takes care of painting e.g. the highlighted for selected
    // or hovered over items in an ItemView)
    option.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, inOption.widget);

    // needed for horizontal or vertically aligned text
    // to figure out where to render the text in order to follow the requested alignment
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option);
    QSize documentSize(doc.size().width(), doc.size().height()); // Convert QSizeF to QSize
    QRect layoutRect = QStyle::alignedRect(Qt::LayoutDirectionAuto, option.displayAlignment, documentSize, textRect);

    painter->save();

    painter->translate(layoutRect.topLeft());
    doc.drawContents(painter, textRect.translated(-textRect.topLeft()));

    painter->restore();
}

QSize WrapWordAnyItemDelegate::sizeHint(const QStyleOptionViewItem &inOption, const QModelIndex &index) const
{
    // qDebug() << "in sizeHint for: " << index.row() << index.column();
    if (index.column() != 1) {
        return QStyledItemDelegate::sizeHint(inOption, index);
    }
    // must first init option for this specific index
    QStyleOptionViewItem option = inOption;
    initStyleOption(&option, index);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextDocument doc;
    doc.setDefaultTextOption(textOption);
    doc.setPlainText(option.text);
    doc.setTextWidth(option.rect.width());
    doc.setDefaultFont(option.font);
    doc.setDocumentMargin(0);
    QSize res = doc.size().toSize();
    // qDebug() << "sizeHint returns: " << res.width() << res.height();
    return res;
}
