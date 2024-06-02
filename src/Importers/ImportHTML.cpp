/************************************************************************
**
**  Copyright (C) 2015-2024 Kevin B. Hendricks, Stratford, Ontario Canada
**  Copyright (C) 2009-2011 Strahinja Markovic  <strahinja.markovic@gmail.com>
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

#include <memory>
#include <functional>

#include <QtCore>
#include <QDir>
#include <QFileInfo>
#include <QFutureSynchronizer>
#include <QtConcurrent>
#include <QDebug>

#include "BookManipulation/CleanSource.h"
#include "BookManipulation/FolderKeeper.h"
#include "BookManipulation/HTMLMetadata.h"
#include "BookManipulation/XhtmlDoc.h"
#include "Importers/ImportHTML.h"
#include "Parsers/GumboInterface.h"
#include "Parsers/OPFParser.h" // for MetaEntry
#include "Misc/HTMLEncodingResolver.h"
#include "Misc/SettingsStore.h"
#include "Misc/TempFolder.h"
#include "Misc/Utility.h"
#include "ResourceObjects/CSSResource.h"
#include "ResourceObjects/HTMLResource.h"
#include "ResourceObjects/NCXResource.h"
#include "ResourceObjects/Resource.h"
#include "SourceUpdates/PerformHTMLUpdates.h"
#include "SourceUpdates/UniversalUpdates.h"
#include "sigil_constants.h"
#include "sigil_exception.h"

// Constructor;
// The parameter is the file to be imported
ImportHTML::ImportHTML(const QString &fullfilepath)
    :
    Importer(fullfilepath),
    m_IgnoreDuplicates(false),
    m_CachedSource(QString())
{
    SettingsStore ss;
    m_EpubVersion = ss.defaultVersion();
}


void ImportHTML::SetBook(QSharedPointer<Book> book, bool ignore_duplicates)
{
    m_Book = book;
    m_IgnoreDuplicates = ignore_duplicates;
    // update epub version to match the book that was just set
    m_EpubVersion = m_Book->GetConstOPF()->GetEpubVersion();
}


XhtmlDoc::WellFormedError ImportHTML::CheckValidToLoad()
{
    // For HTML & XML documents we will perform a well-formed check
    return XhtmlDoc::WellFormedErrorForSource(LoadSource());
}

// should be call after GetBook to get book paths to what was added
const QStringList& ImportHTML::GetAddedBookPaths()
{
    return m_AddedBookPaths;
}

// Reads and parses the file
// and returns the created Book
QSharedPointer<Book> ImportHTML::GetBook(bool extract_metadata)
{
    // First handle any new created Books by making sure there is 
    // an OPF in the current Book
    if (!m_Book->GetConstOPF()) {
        m_Book->GetFolderKeeper()->AddOPFToFolder(m_EpubVersion);
    } 

    QString source = LoadSource();
    if (extract_metadata) {
        LoadMetadata(source);
    }
    UpdateFiles(CreateHTMLResource(), source, LoadFolderStructure(source));

    // Before returning the new book, if it is epub3, make sure it has a nav
    if (m_EpubVersion.startsWith('3')) {
        HTMLResource* nav_resource = m_Book->GetConstOPF()->GetNavResource();
        if (!nav_resource) {
            HTMLResource * nav_resource = m_Book->CreateEmptyNavFile(true);
            m_Book->GetOPF()->SetNavResource(nav_resource);
            m_Book->GetOPF()->SetItemRefLinear(nav_resource, false);
            m_AddedBookPaths << nav_resource->GetRelativePath();
        }
    }

    // Before returning the new book, if it is epub2, make sure it has an ncx
    if (m_EpubVersion.startsWith('2')) {
        NCXResource* ncx_resource = m_Book->GetNCX();
        if (!ncx_resource) {
            // add NCX using "toc.ncx" with id "ncx" right beside the opf
            QString ncxbookpath = Utility::startingDir(m_Book->GetOPF()->GetRelativePath()) + "/toc.ncx";
            ncx_resource = m_Book->GetFolderKeeper()->AddNCXToFolder(m_EpubVersion, ncxbookpath);
            QString NCXId = m_Book->GetOPF()->AddNCXItem(ncx_resource->GetFullPath(),"ncx");
            m_Book->GetOPF()->UpdateNCXOnSpine(NCXId);
            // And properly fill the empty ncx with the default contents to point to the first xhtml file
            QString first_xhtml_bookpath = m_AddedBookPaths.at(0);
            ncx_resource->FillWithDefaultTextToBookPath(m_EpubVersion, first_xhtml_bookpath);
        }
    }
    return m_Book;
}


// Loads the source code into the Book
QString ImportHTML::LoadSource()
{
    SettingsStore ss;

    if (m_CachedSource.isNull()) {
        if (!Utility::IsFileReadable(m_FullFilePath)) {
            throw (CannotReadFile(m_FullFilePath.toStdString()));
        }

        m_CachedSource = HTMLEncodingResolver::ReadHTMLFile(m_FullFilePath);
        m_CachedSource = CleanSource::CharToEntity(m_CachedSource, m_EpubVersion);
        if (ss.cleanOn() & CLEANON_OPEN) {
            m_CachedSource = XhtmlDoc::ResolveCustomEntities(m_CachedSource);
            m_CachedSource = CleanSource::Mend(m_CachedSource, m_EpubVersion);
        }
    }
    return m_CachedSource;
}


// Searches for meta information in the HTML file
// and tries to convert it to Dublin Core
void ImportHTML::LoadMetadata(const QString & source)
{
    GumboInterface gi(source, m_EpubVersion);
    gi.parse();
    QList<MetaEntry> metadata;
    QList<GumboNode*> nodes = gi.get_all_nodes_with_tag(GUMBO_TAG_META); 
    for (int i = 0; i < nodes.count(); ++i) {
        GumboNode* node = nodes.at(i);
        MetaEntry meta = HTMLMetadata::Instance()->MapHTMLToOPFMetadata(node, gi);
        if (!meta.m_name.isEmpty() && !meta.m_content.isEmpty()) {
            metadata.append(meta);
        }
    }
    m_Book->SetMetadata(metadata);
}


HTMLResource *ImportHTML::CreateHTMLResource()
{
    TempFolder tempfolder;
    QString fullfilepath = tempfolder.GetPath() + "/" + QFileInfo(m_FullFilePath).fileName();
    Utility::WriteUnicodeTextFile("TEMP_SOURCE", fullfilepath);
    HTMLResource *resource = qobject_cast<HTMLResource *>(m_Book->GetFolderKeeper()->AddContentFileToFolder(fullfilepath));
    resource->SetCurrentBookRelPath(m_FullFilePath);
    m_AddedBookPaths << resource->GetRelativePath();
    return resource;
}


void ImportHTML::UpdateFiles(HTMLResource *html_resource,
                             QString &source, 
                             const QHash<QString, QString> &updates)
{
    Q_ASSERT(html_resource != NULL);
    QHash<QString, QString> html_updates;
    QHash<QString, QString> css_updates;
    QString newsource = source;
    QString currentpath = html_resource->GetCurrentBookRelPath();
    QString version = html_resource->GetEpubVersion();
    std::tie(html_updates, css_updates, std::ignore) =
        UniversalUpdates::SeparateHtmlCssXmlUpdates(updates);
    QList<Resource *> all_files = m_Book->GetFolderKeeper()->GetResourceList();
    int num_files = all_files.count();
    QList<CSSResource *> css_resources;

    for (int i = 0; i < num_files; ++i) {
        Resource *resource = all_files.at(i);

        if (resource->Type() == Resource::CSSResourceType) {
            css_resources.append(qobject_cast<CSSResource *>(resource));
        }
    }
    
    QFutureSynchronizer<void> sync;
    sync.addFuture(QtConcurrent::map(css_resources,
                                     std::bind(UniversalUpdates::LoadAndUpdateOneCSSFile, std::placeholders::_1, css_updates)));
    QString newbookpath = html_resource->GetRelativePath();

    // add special case to handle just this filename in link (pseudo internal link) with no path
    html_updates[currentpath] = newbookpath;

    // leave untouched any links to non-existing files
    QStringList TargetPaths = XhtmlDoc::GetHrefSrcPaths(newsource);
    QFileInfo hinfo = QFileInfo(m_FullFilePath);
    QDir folder(hinfo.absoluteDir());
    foreach(QString target, TargetPaths) {
        if (target.indexOf(":") == -1) {
            std::pair<QString, QString> parts = Utility::parseRelativeHREF(target);
            QString target_file = hinfo.absolutePath() + "/" + parts.first;
            target_file = Utility::resolveRelativeSegmentsInFilePath(target_file, "/");
            if (!QFile::exists(target_file)) {
                html_updates[target_file] = "";
            } else {
                QString extension = QFileInfo(target_file).suffix();
                // do not touch javascript links when importing html
                // even when they do exist as we do not import them
                if (extension == "js") {
                    html_updates[target_file] = "";
                }
                // we also do not touch links to *other* xhtml files
                if ((target_file != currentpath) &&
                    (extension == "htm" ||
                     extension == "html" ||
                     extension == "xhtml")) {
                     html_updates[target_file] = "";
                }
            }
        }
    }
    html_resource->SetText(PerformHTMLUpdates(newsource, newbookpath, html_updates, css_updates, currentpath, version)());
    html_resource->SetCurrentBookRelPath("");
    sync.waitForFinished();
}


// Loads the referenced files into the main folder of the book;
// as the files get a new name, the references are updated
QHash<QString, QString> ImportHTML::LoadFolderStructure(const QString &source)
{
    QStringList mediapaths = XhtmlDoc::GetPathsToMediaFiles(source);
    QStringList stylepaths = XhtmlDoc::GetPathsToStyleFiles(source);
    QFutureSynchronizer<QHash<QString, QString>> sync;
    sync.addFuture(QtConcurrent::run(&ImportHTML::LoadMediaFiles, this,  mediapaths));
    sync.addFuture(QtConcurrent::run(&ImportHTML::LoadStyleFiles, this,  stylepaths));
    sync.waitForFinished();
    QList<QFuture<QHash<QString, QString>>> futures = sync.futures();
    int num_futures = futures.count();
    QHash<QString, QString> updates;

    for (int i = 0; i < num_futures; ++i) {
        updates.insert(futures.at(i).result());
    }

    return updates;
}


// note file_paths here are hrefs to media files from the html file being imported 
// that should be imported as well

QHash<QString, QString> ImportHTML::LoadMediaFiles(const QStringList & file_paths)
{
    QHash<QString, QString> updates;
    QFileInfo hinfo = QFileInfo(m_FullFilePath);
    QDir folder(hinfo.absoluteDir());
    // Load the media files (images, video, audio) into the book and
    // update all references with new urls
    foreach(QString file_path, file_paths) {
        try {
            QString filename = QFileInfo(file_path).fileName();
            QString fullfilepath  = QFileInfo(folder, file_path).absoluteFilePath();
            QString newpath;
            QString existing_book_path = m_Book->GetFolderKeeper()->GetBookPathByPathEnd(filename);

            if (m_IgnoreDuplicates && !existing_book_path.isEmpty()) {
                newpath = newpath = existing_book_path;
            } else {
                Resource * resource = m_Book->GetFolderKeeper()->AddContentFileToFolder(fullfilepath);
                newpath = resource->GetRelativePath();
                m_AddedBookPaths << newpath;
            }

            updates[ fullfilepath ] = newpath;
        } catch (FileDoesNotExist&) {
            // Do not touch link if it is already broken
            QString target_file = hinfo.absolutePath() + "/" + file_path;
            target_file = Utility::resolveRelativeSegmentsInFilePath(target_file, "/");
            updates[target_file] = "";
            // Do nothing. If the referenced file does not exist,
            // well then we don't load it.
            // qDebug() << "broken link ImportHTML" << m_FullFilePath << file_path << target_file;
        }
    }
    return updates;
}


QHash<QString, QString> ImportHTML::LoadStyleFiles(const QStringList & file_paths )
{
    QHash<QString, QString> updates;
    QFileInfo hinfo = QFileInfo(m_FullFilePath);
    QDir folder(hinfo.absoluteDir());
    foreach(QString file_path, file_paths) {
        try {
            QString filename = QFileInfo(file_path).fileName();
            QString fullfilepath  = QFileInfo(folder, file_path).absoluteFilePath();
            QString newpath;
            QString existing_book_path = m_Book->GetFolderKeeper()->GetBookPathByPathEnd(filename);

            if (m_IgnoreDuplicates && !existing_book_path.isEmpty()) {
                newpath = existing_book_path;
            } else {
                Resource * resource = m_Book->GetFolderKeeper()->AddContentFileToFolder(fullfilepath);
                newpath = resource->GetRelativePath();
                m_AddedBookPaths << newpath;
            }

            updates[ fullfilepath ] = newpath;
        } catch (FileDoesNotExist&) {
            // Do not touch link if it is already broken
            QString target_file = hinfo.absolutePath() + "/" + file_path;
            target_file = Utility::resolveRelativeSegmentsInFilePath(target_file, "/");
            updates[target_file] = "";
            // Do nothing. If the referenced file does not exist,
            // well then we don't load it.
            // qDebug() << "broken link ImportHTML" << m_FullFilePath << file_path << target_file;
        }
    }

    return updates;
}
