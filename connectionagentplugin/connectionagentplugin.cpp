/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@gmail.com
**
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
****************************************************************************/

#include "connectionagentplugin.h"
#include "connectionamanagerinterface.h"

#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>

//#include "notifications/notificationmanager.h"

#include <qobject.h>

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"

ConnectionAgentPlugin::ConnectionAgentPlugin(QObject *parent):
    QObject(parent)
{
    connectiondWatcher = new QDBusServiceWatcher(CONND_SERVICE,QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration |
            QDBusServiceWatcher::WatchForUnregistration, this);

    connect(connectiondWatcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(connectToConnectiond(QString)));
    connect(connectiondWatcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(connectiondUnregistered(QString)));

    connectToConnectiond();
}

ConnectionAgentPlugin::~ConnectionAgentPlugin()
{
}

void ConnectionAgentPlugin::sendUserReply(const QVariantMap &input)
{
    QDBusPendingReply<> reply = connManagerInterface->sendUserReply(input);
    if (reply.isError()) {
     qDebug() << Q_FUNC_INFO << reply.error().message();
    }
}

void ConnectionAgentPlugin::sendConnectReply(const QString &replyMessage, int timeout)
{
     connManagerInterface->sendConnectReply(replyMessage,timeout);
}

void ConnectionAgentPlugin::connectToType(const QString &type)
{
    qDebug() << Q_FUNC_INFO << type;;
    connManagerInterface->connectToType(type);
}

void ConnectionAgentPlugin::onErrorReported(const QString &error)
{
    Q_EMIT errorReported(error);
}

void ConnectionAgentPlugin::onRequestBrowser(const QString &url)
{
    qDebug() << Q_FUNC_INFO <<url;
}

void ConnectionAgentPlugin::onUserInputRequested(const QString &service, const QVariantMap &fields)
{
    // we do this as qtdbus does not understand QVariantMap very well.
    // we need to manually demarshall
    QVariantMap map;
    QMapIterator<QString, QVariant> i(fields);
    // this works for Passphrase at least. anything else?
    while (i.hasNext()) {
        i.next();
        QDBusArgument arg = fields.value(i.key()).value<QDBusArgument>();
        QVariantMap vmap = qdbus_cast<QVariantMap>(arg);
        map.insert(i.key(), vmap);
    }
    Q_EMIT userInputRequested(service, map);
}

void ConnectionAgentPlugin::onConnectionRequested()
{
    Q_EMIT connectionRequest();
}

void ConnectionAgentPlugin::connectToConnectiond(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }

    bool available = QDBusConnection::sessionBus().interface()->isServiceRegistered(CONND_SERVICE);

    if(!available) {
        QDBusReply<void> reply = QDBusConnection::sessionBus().interface()->startService(CONND_SERVICE);
        if (!reply.isValid()) {
            qDebug() << Q_FUNC_INFO << reply.error().message();
            return;
        }
    }

    connManagerInterface = new com::jolla::Connectiond(CONND_SERVICE, CONND_PATH, QDBusConnection::sessionBus(), this);

    connect(connManagerInterface,SIGNAL(connectionRequest()),
            this,SLOT(onConnectionRequested()));
    connect(connManagerInterface,SIGNAL(wlanConfigurationNeeded()),
            this,SIGNAL(wlanConfigurationNeeded()));

    connect(connManagerInterface,SIGNAL(userInputCanceled()),
            this,SIGNAL(userInputCanceled()));

    connect(connManagerInterface,SIGNAL(errorReported(QString)),
                     this,SLOT(onErrorReported(QString)));

    connect(connManagerInterface,SIGNAL(requestBrowser(QString)),
                     this,SLOT(onRequestBrowser(QString)));

    connect(connManagerInterface,SIGNAL(userInputRequested(QString,QVariantMap)),
                     this,SLOT(onUserInputRequested(QString,QVariantMap)), Qt::UniqueConnection);
}

void ConnectionAgentPlugin::connectiondUnregistered(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }
}

void ConnectionAgentPlugin::onWlanConfigurationNeeded()
{
    qDebug() << Q_FUNC_INFO;
    Q_EMIT wlanConfigurationNeeded();
}