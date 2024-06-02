/************************************************************************
**
**  Copyright (C) 2019-2024 Kevin B. Hendricks, Stratford Ontario Canada
**  Copyright (C) 2019-2023 Doug Massay
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

#include <QEvent>
#include <QEventLoop>
#include <QDeadlineTimer>
#include <QTimer>
#include <QSize>
#include <QUrl>
#include <QDir>
#include <QtWebEngineWidgets>
#include <QtWebEngineCore>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWebEngineScript>
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>

#include "MainUI/MainApplication.h"
#include "Misc/WebProfileMgr.h"
#include "Misc/SettingsStore.h"
#include "Misc/Utility.h"
#include "sigil_constants.h"
#include "ViewEditors/WebEngPage.h"
#include "ViewEditors/ViewPreview.h"
#include "ViewEditors/Overlay.h"

#define DBG if(0)

const QString SET_CURSOR_JS2 =
    "var range = document.createRange();"
    "range.setStart(element, 0);"
    "range.setEnd(element, 0);"
    "var selection = window.getSelection();"
    "selection.removeAllRanges();"
    "selection.addRange(range);";

const QString SET_PREVIEW_COLORS =
    "document.body.style.backgroundColor=\"%1\"; "
    "document.body.style.color=\"%2\";";


struct JSResult {
  QVariant res;
  bool     finished;
  JSResult() : res(QVariant("")), finished(false) {}
  JSResult(JSResult * pRes) : res(pRes->res),  finished(pRes->finished) {}
  ~JSResult() { }
  bool isFinished() { return finished; }
};

struct SetJavascriptResultFunctor {
    JSResult * pres;
    SetJavascriptResultFunctor(JSResult * pres) : pres(pres) {}
    void operator()(const QVariant &result) {
        pres->res.setValue(result);
        pres->finished = true;
        DBG qDebug() << "javascript done";
    }
};

struct HTMLResult {
   QString res;
   bool finished;
   HTMLResult() : res(QString("")), finished(false) {}
   HTMLResult(HTMLResult * pRes) : res(pRes->res),  finished(pRes->finished) {}
   bool isFinished() { return finished; }
 };

 struct SetToHTMLResultFunctor {
     HTMLResult * pres;
     SetToHTMLResultFunctor(HTMLResult * pres) : pres(pres) {}
     void operator()(const QString &result) {
         pres->res = result;
         pres->finished = true;
     }
 };

ViewPreview::ViewPreview(QWidget *parent, bool setbackground)
    : QWebEngineView(parent),
      m_isLoadFinished(false),
      c_jQuery(Utility::ReadUnicodeTextFile(":/javascript/jquery-3.6.4.min.js")),
      c_jQueryScrollTo(Utility::ReadUnicodeTextFile(":/javascript/jquery.scrollTo-2.1.2-min.js")),
      c_GetCaretLocation(Utility::ReadUnicodeTextFile(":/javascript/book_view_current_location.js")),
      m_CaretLocationUpdate(QString()),
      m_CustomSetDocumentInProgress(false),
      m_pendingScrollToFragment(QString()),
      m_LoadOkay(false),
      m_overlay(new LoadingOverlay(this))
{
    QWebEngineProfile* profile = WebProfileMgr::instance()->GetPreviewProfile();
    m_ViewWebPage = new WebEngPage(profile, this, setbackground);
    setPage(m_ViewWebPage);
    
    setContextMenuPolicy(Qt::CustomContextMenu);
    // Set the Zoom factor but be sure no signals are set because of this.
    SettingsStore ss;
    SetCurrentZoomFactor(ss.zoomPreview());
    
    // Update the Preview's Profile Settings with User's Preferences
    m_ViewWebPage->profile()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                                       (ss.javascriptOn() == 1));
    m_ViewWebPage->profile()->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows,
                                                       (ss.javascriptOn() == 1));
    m_ViewWebPage->profile()->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls,
                                                       (ss.remoteOn() == 1));

    ConnectSignalsToSlots();
}

ViewPreview::~ViewPreview()
{
    if (m_ViewWebPage != NULL) {
        delete m_ViewWebPage;
        m_ViewWebPage = 0;
    }
}

QString ViewPreview::GetCaretLocationUpdate()
{
    StoreCaretLocationUpdate(GetCaretLocation());
    return m_CaretLocationUpdate;
}

// Pick a sane/modest initial size for the Preview dockwidget
QSize ViewPreview::sizeHint() const
{
    return QSize(200, 400);
}

void ViewPreview::CustomSetDocument(const QString &path, const QString &html)
{
    if (html.isEmpty()) {
        return;
    }

    m_CustomSetDocumentInProgress = true;

    // If this is not the very first load of this document, store the caret location
    if (!url().isEmpty()) {

        // This next line really causes problems as it happens to interfere with later loading
        // StoreCurrentCaretLocation();
 
        // keep memory footprint small clear any caches when a new page loads
        if (url().toLocalFile() != path) {
	    page()->profile()->clearHttpCache();
        } 
    }

    m_isLoadFinished = false;

    // If Tidy is turned off, then Sigil will explode if there is no xmlns
    // on the <html> element. So we will silently add it if needed to ensure
    // no errors occur, to allow loading of documents created outside of
    // Sigil as well as catering for section splits etc.
    QString replaced_html = html;
    replaced_html = replaced_html.replace("<html>", "<html xmlns=\"http://www.w3.org/1999/xhtml\">");
    MainApplication *mainApplication = qobject_cast<MainApplication *>(qApp);
    QString key = Utility::CreateUUID();
    mainApplication->saveInPreviewCache(key, replaced_html);
    QUrl tgturl = QUrl::fromLocalFile(path);
    tgturl.setScheme("sigil");
    tgturl.setHost("");
    tgturl.setQuery("sigilpreview=" + key); 
    page()->load(tgturl);
    // setContent(replaced_html.toUtf8(), "application/xhtml+xml;charset=UTF-8", QUrl::fromLocalFile(path));
}

bool ViewPreview::IsLoadingFinished()
{
    return m_isLoadFinished;
}

QString ViewPreview::GetHoverUrl()
{
    return m_hoverUrl;
}

void ViewPreview::SetZoomFactor(float factor)
{
    SettingsStore settings;
    settings.setZoomPreview(factor);
    SetCurrentZoomFactor(factor);
    Zoom();
    emit ZoomFactorChanged(factor);
}

void ViewPreview::SetCurrentZoomFactor(float factor)
{
    m_CurrentZoomFactor = factor;
}

float ViewPreview::GetZoomFactor() const
{
    return m_CurrentZoomFactor;
}

void ViewPreview::Zoom()
{
    setZoomFactor(m_CurrentZoomFactor);
}

void ViewPreview::UpdateDisplay()
{
#if 0
    SettingsStore settings;
    float stored_factor = settings.zoomWeb();

    if (stored_factor != m_CurrentZoomFactor) {
        m_CurrentZoomFactor = stored_factor;
        Zoom();
    }
#endif
}

void ViewPreview::SetPreviewColors(const QString &bg, const QString &fg)
{
    DoJavascript(SET_PREVIEW_COLORS.arg(bg).arg(fg));
}

void ViewPreview::ScrollToTop()
{
    QString caret_location = "var elementList = document.getElementsByTagName(\"body\");"
                             "var element = elementList[0];";
    QString scroll = "var from_top = window.innerHeight / 2;"
                     "if (typeof element !== 'undefined') {"
                     "    $.scrollTo(element, 0, {offset: {top:-from_top, left:0} });";
    QString script = caret_location + scroll + SET_CURSOR_JS2 + "}";

    DoJavascript(script);
}

void ViewPreview::ScrollToFragment(const QString &fragment)
{
    if (IsLoadingFinished()) {
        ScrollToFragmentInternal(fragment);
    } else {
        m_pendingScrollToFragment = fragment;
    }
}

void ViewPreview::ScrollToFragmentInternal(const QString &fragment)
{
    if (fragment.isEmpty()) {
        ScrollToTop();
        return;
    }
    QString caret_location = "var element = document.getElementById(\"" % fragment % "\");";
    QString scroll = "var from_top = window.innerHeight / 2.5;"
                     "if (typeof element !== 'undefined') {"
                     "$.scrollTo(element, 0, {offset: {top:-from_top, left:0 } });";
    QString script = caret_location + scroll + SET_CURSOR_JS2 + "}";
    DoJavascript(script);
}

void ViewPreview::LinkHovered(const QString &url)
{
    DBG qDebug() << "linkHovered received " << url;;
    m_hoverUrl = url;
}

void ViewPreview::LoadingStarted()
{
    DBG qDebug() << "Loading a page started";
    m_isLoadFinished = false;
    m_LoadOkay = false;
    ShowOverlay();
}


// AAARRRGGGHHHH - when a user hits a link that is a fragment
// that points to the current page, a load started signal is 
// generated but NO proper load finished signal ever comes.
// This is a horrible Qt bug to exist in a Qt 5.12.3 LTS release

// To workaround this keep track of our loading state
// and set it to finished when progress hits 100
void ViewPreview::LoadingProgress(int progress)
{
    DBG qDebug() << "Loading progress " << progress;
    if (progress >= 100 && !m_CustomSetDocumentInProgress) {
        m_isLoadFinished = true;
        m_LoadOkay = true;
    }
    if (zoomFactor() != m_CurrentZoomFactor) {
        setZoomFactor(m_CurrentZoomFactor);
    }
    emit ViewProgress(progress);
}

void ViewPreview::UpdateFinishedState(bool okay)
{
    // AAArrrrggggggghhhhhhhh - Qt 5.12.2 has a bug that returns 
    // loadFinished with okay set to false when caused by 
    // clicking a link that acceptNavigationRequest denies
    // even when there are no apparent errors!

    DBG qDebug() << "UpdateFinishedState with okay " << okay;
    // m_isLoadFinished = true;
    m_LoadOkay = okay;
    HideOverlay();
}

void ViewPreview::HideOverlay()
{
    m_overlay->hide();
}

void ViewPreview::ShowOverlay()
{
    m_overlay->show();
}

QString ViewPreview::GetHTML() const 
 {
     HTMLResult * pres = new HTMLResult();
     page()->toHtml(SetToHTMLResultFunctor(pres));
     while(!pres->isFinished()) {
         qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers, 100);
     }
     QString res = pres->res;
     delete pres;
     return res;
 }


QVariant ViewPreview::EvaluateJavascript(const QString &javascript)
{
    // DBG qDebug() << "EvaluateJavascript: " << m_isLoadFinished;

    // do not try to evaluate javascripts with the page not loaded yet
    if (!m_isLoadFinished) return QVariant();

    JSResult * pres = new JSResult();
    QDeadlineTimer deadline(10000);  // in milliseconds

    DBG qDebug() << "evaluate javascript" << javascript;

    page()->runJavaScript(javascript,QWebEngineScript::ApplicationWorld, SetJavascriptResultFunctor(pres));
    while(!pres->isFinished() && (!deadline.hasExpired())) {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
    }
    QVariant res;
    if (pres->isFinished()) {
        res = pres->res;
        delete pres;
    } else {
        qDebug() << "Error: VP EvaluateJavascript timed out";
        qDebug() << "   ... intentionally leaking the JSResult structure";
    }
    return res;
}

void ViewPreview::DoJavascript(const QString &javascript)
{
    DBG qDebug() << "DoJavascript with " << m_isLoadFinished;
    // DBG qDebug() << "In DoJavaScript with: " << javascript;

     // do not try to evaluate javascripts with the page not loaded yet
    if (!m_isLoadFinished) return;

    page()->runJavaScript(javascript,QWebEngineScript::ApplicationWorld);
}


// If we don't steal focus first, then the QWebView
// can have focus (and its QWebFrame) and still not
// really have it (no blinking cursor).
//   We also still need to attempt to GrabFocus even
// when shown as a Preview page (even though no cursor
// is shown) or else the QStackWidget will explode on
// Windows when switching to another tab when it tries
// to determine where the previous focus was.
void ViewPreview::GrabFocus()
{
    // qobject_cast<QWidget *>(parent())->setFocus();
    QWebEngineView::setFocus();
}

// do not set load to finished until jqery has been loaded
void ViewPreview::WebPageJavascriptOnLoad()
{
    DBG qDebug() << "WebPageJavascriptOnLoad start";
    page()->runJavaScript(c_jQuery, QWebEngineScript::ApplicationWorld);
    page()->runJavaScript(c_jQueryScrollTo, QWebEngineScript::ApplicationWorld);
    DBG qDebug() << "WebPageJavascriptOnLoad end";
    DBG qDebug() << "WebPageJavascriptOnLoad with m_CustomSetDocumentInProgress: " << m_CustomSetDocumentInProgress;
    m_isLoadFinished = true;
    if (m_CustomSetDocumentInProgress) {
        // setZoomFactor(m_CurrentZoomFactor);
        if (!m_pendingScrollToFragment.isEmpty()) {
            ScrollToFragment(m_pendingScrollToFragment);
            m_pendingScrollToFragment.clear();
        } else {
            // Zoom must be complete before scrolling to an element and centering on it
            // *but* is not instantaneous.
            // It is better to delay this and handle it in PreviewWindow
            // executeCaretUpdateInternal();
        }
        m_CustomSetDocumentInProgress = false;
    }
    emit DocumentLoaded();
}

QString ViewPreview::GetElementSelectingJS_NoTextNodes(const QList<ElementIndex> &hierarchy) const
{
    // TODO: see if replacing jQuery with pure JS will speed up
    // caret location scrolling... note the children()/contents() difference:
    // children() only considers element nodes, contents() considers text nodes too.
    QString element_selector = "$('html')";
    // We will have a different hierarchy depending on whether it was generated by CodeView or
    // generated by Preview. If the last element is '#text', strip it off and make sure the
    // element preceding it has a -1 index in order for the caret to move correctly.
    bool adjust_last_index = false;
    int hierarchy_length = hierarchy.count() - 1;

    if (hierarchy_length > 1 && hierarchy.last().name == "#text") {
        hierarchy_length--;
    }

    for (int i = 0; i < hierarchy_length; ++i) {
        int index = hierarchy[ i ].index;

        if ((i == hierarchy_length) && adjust_last_index) {
            index = -1;
        }

        element_selector.append(QString(".children().eq(%1)").arg(index));
    }

    element_selector.append(".get(0)");
    return element_selector;
}

QList<ElementIndex> ViewPreview::GetCaretLocation()
{
    // The location element hierarchy encoded in a string
    QString location_string = EvaluateJavascript(c_GetCaretLocation).toString();
    DBG qDebug() << "GetCaretLocation: " << location_string;
    return ConvertQWebPathToHierarchy(location_string);
}

QList<ElementIndex> ViewPreview::ConvertQWebPathToHierarchy(const QString & webpath) const
{
    // The location element hierarchy encoded in a string
    QString location_string = webpath;
    QStringList elements    = location_string.split(",", Qt::SkipEmptyParts);
    QList<ElementIndex> location;
    foreach(QString element, elements) {
        ElementIndex new_element;
        new_element.name  = element.split(" ")[ 0 ];
        new_element.index = element.split(" ")[ 1 ].toInt();
        location.append(new_element);
    }
    return location;
}

QString ViewPreview::ConvertHierarchyToQWebPath(const QList<ElementIndex>& hierarchy)
{
    QStringList pathparts;
    for (int i=0; i < hierarchy.count(); i++) {
        QString part = hierarchy.at(i).name + " " + QString::number(hierarchy.at(i).index);
        pathparts.append(part);
    }
    return pathparts.join(",");
}

void ViewPreview::StoreCurrentCaretLocation()
{
    // Only overwrite the current location stored if it is empty, in case we specifically
    // want a new location when switching to a new view
    if (m_CaretLocationUpdate.isEmpty()) {
        StoreCaretLocationUpdate(GetCaretLocation());
    }
}

void ViewPreview::StoreCaretLocationUpdate(const QList<ElementIndex> &hierarchy)
{
    DBG qDebug() << "in StoreCaretLocationUpdate";
    QString caret_location = "var element = " + GetElementSelectingJS_NoTextNodes(hierarchy) + ";";
    // We scroll to the element and center the screen on it
    QString scroll = "var from_top = window.innerHeight / 2;"
                     "if (typeof element !== 'undefined') {"
                     "$.scrollTo( element, 0, {offset: {top:-from_top, left:0 } } );";
    m_CaretLocationUpdate = caret_location + scroll + SET_CURSOR_JS2 + "}";
}

QString ViewPreview::GetElementSelectingJS_WithTextNode(const QList<ElementIndex> &hierarchy) const
{
    QString element_selector = "$('html')";

    for (int i = 0; i < hierarchy.count() - 1; ++i) {
        element_selector.append(QString(".children().eq(%1)").arg(hierarchy[ i ].index));
    }

    element_selector.append(QString(".contents().eq(%1)").arg(hierarchy.last().index));
    element_selector.append(".get(0)");
    return element_selector;
}


bool ViewPreview::ExecuteCaretUpdate()
{

    // If there is no caret location update pending...
    if (m_CaretLocationUpdate.isEmpty()) {
        return false;
    }

    // run it...
    DoJavascript(m_CaretLocationUpdate);
    // ...and clear the update.
    m_CaretLocationUpdate.clear();
    return true;
}

bool ViewPreview::ExecuteCaretUpdate(const QString &caret_update)
{
    // This overload is for use with the Back To Link type functionality, where we have
    // a specific caret location javascript we want to force when the tab is fully loaded.
    if (!caret_update.isEmpty()) {
        m_CaretLocationUpdate = caret_update;
        return ExecuteCaretUpdate();
    }

    return false;
}


void ViewPreview::ConnectSignalsToSlots()
{
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(UpdateFinishedState(bool)));
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(WebPageJavascriptOnLoad()));
    connect(page(), SIGNAL(loadStarted()), this, SLOT(LoadingStarted()));
    connect(page(), SIGNAL(LinkClicked(const QUrl &)), this, SIGNAL(LinkClicked(const QUrl &)));
    connect(page(), SIGNAL(loadProgress(int)), this, SLOT(LoadingProgress(int)));
    connect(page(), SIGNAL(linkHovered(const QString &)), this, SLOT(LinkHovered(const QString &)));
}
