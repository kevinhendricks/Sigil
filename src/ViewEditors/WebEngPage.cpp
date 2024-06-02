/************************************************************************
**
**  Copyright (C) 2019-2024 Kevin B. Hendricks, Stratford Ontario Canada
**  Copyright (C) 2023- Doug Massay
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
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QWebEngineProfile>
#include "Misc/Utility.h"
#include "ViewEditors/WebEngPage.h"

#define DBG if(0)
 
WebEngPage::WebEngPage(QWebEngineProfile* profile, QObject *parent, bool setbackground)
    : QWebEnginePage(profile, parent)
{
    if (setbackground) {
        setBackgroundColor(Utility::WebViewBackgroundColor(true));
    }
    setUrl(QUrl("about:blank"));
#if 0
    DBG qDebug() << "WebEngPage Life Cycle State: " << lifecycleState();
    DBG qDebug() << "WebEngPage Current RenderProcess Pid " << renderProcessPid();
    DBG qDebug() << "WebEngPage Current RecommendedState " << recommendedState(); 

    connect(this, SIGNAL(lifecycleStateChanged(QWebEnginePage::LifecycleState)),
	    this, SLOT(lifecyclechange(QWebEnginePage::LifecycleState)));
    connect(this, SIGNAL(renderProcessPidChanged(qint64)), this, SLOT(render_new_pid(qint64)));
    connect(this, SIGNAL(renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus, int)),
	    this, SLOT(render_died(QWebEnginePage::RenderProcessTerminationStatus, int)));
#endif
}


// Because you can not delegate all links in QtWebEngine we must override here and generate
// our own link requests

// BUT a loadStarted signal is emitted by this page **before** this is called
// Even **before** it knows how we want to handle it!
// Once we "return false" from this a loadFinished with okay **false** is generated.

// These false loadStart and LoadFinished greatly confuse our model

// Therefore do NOT emit a signal from this method as it can create huge delays in when
// loadFinished(okay) returns (with okay as false) 

// AAARRRRRGGGGGGHHHHHHH, this routine is *NOT* invoked when a user clicks on an
// internal link (ie. a fragment to a different location in the current page)

bool WebEngPage::acceptNavigationRequest(const QUrl & url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    DBG qDebug() << "acceptNavigationRequest " << url.toString() << " , " << type << " , " << isMainFrame;
    if ((type == QWebEnginePage::NavigationTypeLinkClicked) || (type == QWebEnginePage::NavigationTypeOther)) {
        if (isMainFrame) {
            m_url = url;
            QTimer::singleShot(20,this,SLOT(EmitLinkClicked()));
            return false;
        }
        // allow secondary frames such as iframes to be loaded automatically
        return true;
    }
    if (type == QWebEnginePage::NavigationTypeTyped) {
        DBG qDebug() << "acceptNavigationRequest from scheme handler load" << url.toString();
        return true;
    }
    if (type == QWebEnginePage::NavigationTypeRedirect) {
        DBG qDebug() << "acceptNavigationRequest from scheme handler redirect" << url.toString();
        return true;
    }
    qDebug() << " Unhandled acceptNavigationRequest with type: " << type;
    return true;
}

void WebEngPage::EmitLinkClicked()
{
    emit LinkClicked(m_url);
}

void WebEngPage::javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level, 
                  const QString & message, int lineNumber, const QString & sourceID)
{
    const QString logEntry = message + " on line:" % QString::number(lineNumber) % " Source:" + sourceID;
    qDebug() << "Javascript error: " << level << logEntry;
}

#if 0
// Keep this around to help with debugging
void WebEngPage::render_new_pid(qint64 pid)
{
    DBG qDebug() << "*** Render Process PID Changed: " << pid;
}

void WebEngPage::render_died(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    DBG qDebug() << "*** Render Porcess Terminated: " << terminationStatus << exitCode;
}

void WebEngPage::lifecyclechange(QWebEnginePage::LifecycleState state)
{
    DBG qDebug() << "*** life cycle change: " << state;
}
#endif
