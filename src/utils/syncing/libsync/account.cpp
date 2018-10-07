/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "account.h"
#include "cookiejar.h"
#include "networkjobs.h"
#include "configfile.h"
#include "accessmanager.h"
#include "creds/abstractcredentials.h"
#include "capabilities.h"
#include "theme.h"

#include "common/asserts.h"
#include "clientsideencryption.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QNetworkCookieJar>

#include <QFileInfo>
#include <QDir>
#include <QSslKey>
#include <QAuthenticator>
#include <QStandardPaths>

namespace OCC {

Q_LOGGING_CATEGORY(lcAccount, "nextcloud.sync.account", QtInfoMsg)

Account::Account(QObject *parent)
    : QObject(parent)
    , _capabilities(QVariantMap())
    , _davPath(Theme::instance()->webDavPath())
{
    qRegisterMetaType<AccountPtr>("AccountPtr");
}

AccountPtr Account::create()
{
    AccountPtr acc = AccountPtr(new Account);
    acc->setSharedThis(acc);

		//TODO: This probably needs to have a better
		// coupling, but it should work for now.
		acc->e2e()->setAccount(acc);
    return acc;
}

ClientSideEncryption* Account::e2e()
{
	// Qt expects everything in the connect to be a pointer, so return a pointer.
	return &_e2e;
}

Account::~Account()
{
}

QString Account::davPath() const
{
    if (capabilities().chunkingNg()) {
        // The chunking-ng means the server prefer to use the new webdav URL
        return QLatin1String("/remote.php/dav/files/") + davUser() + QLatin1Char('/');
    }

    // make sure to have a trailing slash
    if (!_davPath.endsWith('/')) {
        QString dp(_davPath);
        dp.append('/');
        return dp;
    }
    return _davPath;
}

void Account::setSharedThis(AccountPtr sharedThis)
{
    _sharedThis = sharedThis.toWeakRef();
}

AccountPtr Account::sharedFromThis()
{
    return _sharedThis.toStrongRef();
}

QString Account::davUser() const
{
    return _davUser.isEmpty() ? _credentials->user() : _davUser;
}

void Account::setDavUser(const QString &newDavUser)
{
    _davUser = newDavUser;
}

#ifndef TOKEN_AUTH_ONLY
QImage Account::avatar() const
{
    return _avatarImg;
}
void Account::setAvatar(const QImage &img)
{
    _avatarImg = img;
    emit accountChangedAvatar();
}
#endif

QString Account::displayName() const
{
    QString dn = QString("%1@%2").arg(davUser(), _url.host());
    int port = url().port();
    if (port > 0 && port != 80 && port != 443) {
        dn.append(QLatin1Char(':'));
        dn.append(QString::number(port));
    }
    return dn;
}

QString Account::davDisplayName() const
{
    return _displayName;
}

void Account::setDavDisplayName(const QString &newDisplayName)
{
    _displayName = newDisplayName;
    emit accountChangedDisplayName();
}

QString Account::id() const
{
    return _id;
}

AbstractCredentials *Account::credentials() const
{
    return _credentials.data();
}

void Account::setCredentials(AbstractCredentials *cred)
{
    // set active credential manager
    QNetworkCookieJar *jar = 0;
    if (_am) {
        jar = _am->cookieJar();
        jar->setParent(0);

        _am = QSharedPointer<QNetworkAccessManager>();
    }

    // The order for these two is important! Reading the credential's
    // settings accesses the account as well as account->_credentials,
    _credentials.reset(cred);
    cred->setAccount(this);

    // Note: This way the QNAM can outlive the Account and Credentials.
    // This is necessary to avoid issues with the QNAM being deleted while
    // processing slotHandleSslErrors().
    _am = QSharedPointer<QNetworkAccessManager>(_credentials->createQNAM(), &QObject::deleteLater);

    if (jar) {
        _am->setCookieJar(jar);
    }
    connect(_am.data(), SIGNAL(sslErrors(QNetworkReply *, QList<QSslError>)),
        SLOT(slotHandleSslErrors(QNetworkReply *, QList<QSslError>)));
    connect(_am.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
    connect(_credentials.data(), &AbstractCredentials::fetched,
        this, &Account::slotCredentialsFetched);
    connect(_credentials.data(), &AbstractCredentials::asked,
        this, &Account::slotCredentialsAsked);
}

QUrl Account::davUrl() const
{
    return Utility::concatUrlPath(url(), davPath());
}

QUrl Account::deprecatedPrivateLinkUrl(const QByteArray &numericFileId) const
{
    return Utility::concatUrlPath(_userVisibleUrl,
        QLatin1String("/index.php/f/") + QUrl::toPercentEncoding(QString::fromLatin1(numericFileId)));
}

/**
 * clear all cookies. (Session cookies or not)
 */
void Account::clearCookieJar()
{
    auto jar = qobject_cast<CookieJar *>(_am->cookieJar());
    ASSERT(jar);
    jar->setAllCookies(QList<QNetworkCookie>());
    emit wantsAccountSaved(this);
}

/*! This shares our official cookie jar (containing all the tasty
    authentication cookies) with another QNAM while making sure
    of not losing its ownership. */
void Account::lendCookieJarTo(QNetworkAccessManager *guest)
{
    auto jar = _am->cookieJar();
    auto oldParent = jar->parent();
    guest->setCookieJar(jar); // takes ownership of our precious cookie jar
    jar->setParent(oldParent); // takes it back
}

QString Account::cookieJarPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/cookies" + id() + ".db";
}

void Account::resetNetworkAccessManager()
{
    if (!_credentials || !_am) {
        return;
    }

    qCDebug(lcAccount) << "Resetting QNAM";
    QNetworkCookieJar *jar = _am->cookieJar();

    // Use a QSharedPointer to allow locking the life of the QNAM on the stack.
    // Make it call deleteLater to make sure that we can return to any QNAM stack frames safely.
    _am = QSharedPointer<QNetworkAccessManager>(_credentials->createQNAM(), &QObject::deleteLater);

    _am->setCookieJar(jar); // takes ownership of the old cookie jar
    connect(_am.data(), SIGNAL(sslErrors(QNetworkReply *, QList<QSslError>)),
        SLOT(slotHandleSslErrors(QNetworkReply *, QList<QSslError>)));
    connect(_am.data(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &Account::proxyAuthenticationRequired);
}

QNetworkAccessManager *Account::networkAccessManager()
{
    return _am.data();
}

QSharedPointer<QNetworkAccessManager> Account::sharedNetworkAccessManager()
{
    return _am;
}

QNetworkReply *Account::sendRawRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    req.setUrl(url);
    req.setSslConfiguration(this->getOrCreateSslConfig());
    if (verb == "HEAD" && !data) {
        return _am->head(req);
    } else if (verb == "GET" && !data) {
        return _am->get(req);
    } else if (verb == "POST") {
        return _am->post(req, data);
    } else if (verb == "PUT") {
        return _am->put(req, data);
    } else if (verb == "DELETE" && !data) {
        return _am->deleteResource(req);
    }
    return _am->sendCustomRequest(req, verb, data);
}

SimpleNetworkJob *Account::sendRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    auto job = new SimpleNetworkJob(sharedFromThis(), this);
    job->startRequest(verb, url, req, data);
    return job;
}

void Account::setSslConfiguration(const QSslConfiguration &config)
{
    _sslConfiguration = config;
}

QSslConfiguration Account::getOrCreateSslConfig()
{
    if (!_sslConfiguration.isNull()) {
        // Will be set by CheckServerJob::finished()
        // We need to use a central shared config to get SSL session tickets
        return _sslConfiguration;
    }

    // if setting the client certificate fails, you will probably get an error similar to this:
    //  "An internal error number 1060 happened. SSL handshake failed, client certificate was requested: SSL error: sslv3 alert handshake failure"
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();

    // Try hard to re-use session for different requests
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionTickets, false);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionSharing, false);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);

    return sslConfig;
}

void Account::setApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts = certs;
}

void Account::addApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts += certs;
}

void Account::resetRejectedCertificates()
{
    _rejectedCertificates.clear();
}

void Account::setSslErrorHandler(AbstractSslErrorHandler *handler)
{
    _sslErrorHandler.reset(handler);
}

void Account::setUrl(const QUrl &url)
{
    _url = url;
    _userVisibleUrl = url;
}

void Account::setUserVisibleHost(const QString &host)
{
    _userVisibleUrl.setHost(host);
}

QVariant Account::credentialSetting(const QString &key) const
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        QString value = _settingsMap.value(prefix + "_" + key).toString();
        if (value.isEmpty()) {
            value = _settingsMap.value(key).toString();
        }
        return value;
    }
    return QVariant();
}

void Account::setCredentialSetting(const QString &key, const QVariant &value)
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        _settingsMap.insert(prefix + "_" + key, value);
    }
}

void Account::slotHandleSslErrors(QNetworkReply *reply, QList<QSslError> errors)
{
    NetworkJobTimeoutPauser pauser(reply);
    QString out;
    QDebug(&out) << "SSL-Errors happened for url " << reply->url().toString();
    foreach (const QSslError &error, errors) {
        QDebug(&out) << "\tError in " << error.certificate() << ":"
                     << error.errorString() << "(" << error.error() << ")"
                     << "\n";
    }

    bool allPreviouslyRejected = true;
    foreach (const QSslError &error, errors) {
        if (!_rejectedCertificates.contains(error.certificate())) {
            allPreviouslyRejected = false;
        }
    }

    // If all certs have previously been rejected by the user, don't ask again.
    if (allPreviouslyRejected) {
        qCInfo(lcAccount) << out << "Certs not trusted by user decision, returning.";
        return;
    }

    QList<QSslCertificate> approvedCerts;
    if (_sslErrorHandler.isNull()) {
        qCWarning(lcAccount) << out << "called without valid SSL error handler for account" << url();
        return;
    }

    // SslDialogErrorHandler::handleErrors will run an event loop that might execute
    // the deleteLater() of the QNAM before we have the chance of unwinding our stack.
    // Keep a ref here on our stackframe to make sure that it doesn't get deleted before
    // handleErrors returns.
    QSharedPointer<QNetworkAccessManager> qnamLock = _am;
    QPointer<QObject> guard = reply;

    if (_sslErrorHandler->handleErrors(errors, reply->sslConfiguration(), &approvedCerts, sharedFromThis())) {
        if (!guard)
            return;

        QSslSocket::addDefaultCaCertificates(approvedCerts);
        addApprovedCerts(approvedCerts);
        emit wantsAccountSaved(this);
        // all ssl certs are known and accepted. We can ignore the problems right away.
        qCInfo(lcAccount) << out << "Certs are known and trusted! This is not an actual error.";

        // Warning: Do *not* use ignoreSslErrors() (without args) here:
        // it permanently ignores all SSL errors for this host, even
        // certificate changes.
        reply->ignoreSslErrors(errors);
    } else {
        if (!guard)
            return;

        // Mark all involved certificates as rejected, so we don't ask the user again.
        foreach (const QSslError &error, errors) {
            if (!_rejectedCertificates.contains(error.certificate())) {
                _rejectedCertificates.append(error.certificate());
            }
        }

        // Not calling ignoreSslErrors will make the SSL handshake fail.
        return;
    }
}

void Account::slotCredentialsFetched()
{
    emit credentialsFetched(_credentials.data());
}

void Account::slotCredentialsAsked()
{
    emit credentialsAsked(_credentials.data());
}

void Account::handleInvalidCredentials()
{
    emit invalidCredentials();
}

void Account::clearQNAMCache()
{
    _am->clearAccessCache();
}

const Capabilities &Account::capabilities() const
{
    return _capabilities;
}

void Account::setCapabilities(const QVariantMap &caps)
{
    _capabilities = Capabilities(caps);
}

QString Account::serverVersion() const
{
    return _serverVersion;
}

int Account::serverVersionInt() const
{
    // FIXME: Use Qt 5.5 QVersionNumber
    auto components = serverVersion().split('.');
    return makeServerVersion(components.value(0).toInt(),
        components.value(1).toInt(),
        components.value(2).toInt());
}

int Account::makeServerVersion(int majorVersion, int minorVersion, int patchVersion)
{
    return (majorVersion << 16) + (minorVersion << 8) + patchVersion;
}

bool Account::serverVersionUnsupported() const
{
    if (serverVersionInt() == 0) {
        // not detected yet, assume it is fine.
        return false;
    }
    return serverVersionInt() < makeServerVersion(9, 1, 0);
}

void Account::setServerVersion(const QString &version)
{
    if (version == _serverVersion) {
        return;
    }

    auto oldServerVersion = _serverVersion;
    _serverVersion = version;
    emit serverVersionChanged(this, oldServerVersion, version);
}

bool Account::rootEtagChangesNotOnlySubFolderEtags()
{
    return (serverVersionInt() >= makeServerVersion(8, 1, 0));
}

void Account::setNonShib(bool nonShib)
{
    if (nonShib) {
        _davPath = Theme::instance()->webDavPathNonShib();
    } else {
        _davPath = Theme::instance()->webDavPath();
    }
}

} // namespace OCC
