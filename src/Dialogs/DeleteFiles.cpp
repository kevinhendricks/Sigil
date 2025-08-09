/************************************************************************
**
**  Copyright (C) 2015-2025 Kevin B. Hendricks, Stratford Ontario Canada
**  Copyright (C) 2012      Dave Heiland
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

#include <QTextDocument>
#include <QTextOption>
#include <QSize>
#include <QTimer>
#include <QtWidgets/QPushButton>
#include "Misc/SettingsStore.h"
#include "Dialogs/WrapWordAnyItemDelegate.h"
#include "Dialogs/DeleteFiles.h"

static const QString SETTINGS_GROUP      = "delete_files";

// files_to_delete are book paths and only bookpaths for safety
DeleteFiles::DeleteFiles(QStringList files_to_delete, QWidget *parent)
    :
    QDialog(parent),
    m_FilesToDelete(files_to_delete),
    m_WrapWordAnyDelegate(new WrapWordAnyItemDelegate())
{
    ui.setupUi(this);
    ConnectSignals();
    ReadSettings();
    foreach(QString filepath, m_FilesToDelete) {
        QList<QStandardItem *> rowItems;
        // Checkbox
        QStandardItem *checkbox_item = new QStandardItem();
        checkbox_item->setCheckable(true);
        checkbox_item->setCheckState(Qt::Checked);
        rowItems << checkbox_item;
        // File Path
        QStandardItem *file_item = new QStandardItem();
        file_item->setText(filepath);
        rowItems << file_item;
        for (int i = 0; i < rowItems.count(); i++) {
            rowItems[i]->setEditable(false);
        }
        m_Model.appendRow(rowItems);
    }
    SetUpTable();
    QTimer::singleShot(100, this, SLOT(SizeRowsForContent()));

}

DeleteFiles::~DeleteFiles()
{
    WriteSettings();
}

void DeleteFiles::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    SizeRowsForContent();
}

// To use an ItemDelegate's sizeHint a QTableView must invoke:
// resizeRowToContents(int row) otherwise it is not used
// This must also be called every time the user resizes the table manually

// Note resizeRowsToContents() never even looks at the Item Delegate sizeHint
// and so is pretty worthless

void DeleteFiles::SizeRowsForContent()
{
    for (int row = 0; row < m_Model.rowCount(); row++) {
        ui.Table->resizeRowToContents(row);
    }
}


void DeleteFiles::SetUpTable()
{
    QStringList header;
    QPushButton *delete_button = ui.buttonBox->button(QDialogButtonBox::Ok);
    delete_button->setText(tr("Delete Marked Files"));
    header.append(tr("Delete"));
    header.append(tr("File"));
    m_Model.setHorizontalHeaderLabels(header);
    ui.Table->setModel(&m_Model);
    ui.Table->setTextElideMode(Qt::ElideNone);
    ui.Table->setWordWrap(true);
    ui.Table->setItemDelegateForColumn(1, m_WrapWordAnyDelegate);
    // Make the header fill all the available space
    ui.Table->horizontalHeader()->setStretchLastSection(true);
    ui.Table->verticalHeader()->setVisible(false);
    ui.Table->setSortingEnabled(true);
    ui.Table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.Table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui.Table->setAlternatingRowColors(true);
    ui.Table->resizeRowsToContents();
}

void DeleteFiles::SaveFilesToDelete()
{
    for (int row = 0; row < m_Model.rowCount(); row++) {
        bool checked = m_Model.itemFromIndex(m_Model.index(row, 0))->checkState() == Qt::Checked;

        if (!checked) {
            QString filepath = m_Model.data(m_Model.index(row, 1)).toString();
            m_FilesToDelete.removeOne(filepath);
        }
    }
}

QStringList DeleteFiles::GetFilesToDelete()
{
    return m_FilesToDelete;
}

void DeleteFiles::ReadSettings()
{
    SettingsStore settings;
    settings.beginGroup(SETTINGS_GROUP);
    // The size of the window and it's full screen status
    QByteArray geometry = settings.value("geometry").toByteArray();

    if (!geometry.isNull()) {
        restoreGeometry(geometry);
    }

    ui.ToggleSelectAll->setCheckState(Qt::Checked);

    settings.endGroup();
}


void DeleteFiles::WriteSettings()
{
    SettingsStore settings;
    settings.beginGroup(SETTINGS_GROUP);
    // The size of the window and it's full screen status
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void DeleteFiles::DoubleClick(const QModelIndex index)
{
    QString filepath = m_Model.item(index.row(), 1)->text();
    // MainWindow OpenFile() will handle a ShortPathName or a Book Path
    // since both are unique
    emit OpenFileRequest(filepath, 1, -1);
}

void DeleteFiles::SelectUnselectAll(bool value)
{
    Qt::CheckState checkboxValue = (value ? Qt::Checked : Qt::Unchecked);
    for (int row = 0; row < m_Model.rowCount(); row++) {
        QStandardItem *checkbox = m_Model.itemFromIndex(m_Model.index(row, 0));
        checkbox->setCheckState(checkboxValue);
    }
}

void DeleteFiles::ConnectSignals()
{
    connect(this, SIGNAL(accepted()), this, SLOT(SaveFilesToDelete()));
    connect(ui.Table, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(DoubleClick(const QModelIndex &)));
    connect(ui.ToggleSelectAll, SIGNAL(clicked(bool)), this, SLOT(SelectUnselectAll(bool)));
}
