/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkReply>
#include <QSettings>
#include <QSslKey>
#include <QJsonObject>
#include <QJsonDocument>
#include <QBuffer>

#include <keychain.h>

#include "account.h"
#include "accessmanager.h"
#include "configfile.h"
#include "theme.h"
#include "syncengine.h"
#include "creds/credentialscommon.h"
#include "creds/httpcredentials.h"
#include <QAuthenticator>

using namespace QKeychain;

namespace OCC {

Q_LOGGING_CATEGORY(lcHttpCredentials, "nextcloud.sync.credentials.http", QtInfoMsg)

namespace {
    const char userC[] = "user";
    const char isOAuthC[] = "oauth";
    const char clientCertificatePEMC[] = "_clientCertificatePEM";
    const char clientKeyPEMC[] = "_clientKeyPEM";
    const char authenticationFailedC[] = "owncloud-authentication-failed";
} // ns

class HttpCredentialsAccessManager : public AccessManager
{
public:
    HttpCredentialsAccessManager(const HttpCredentials *cred, QObject *parent = 0)
        : AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) Q_DECL_OVERRIDE
    {
        QNetworkRequest req(request);
        if (!req.attribute(HttpCredentials::DontAddCredentialsAttribute).toBool()) {
            if (_cred && !_cred->password().isEmpty()) {
                if (_cred->isUsingOAuth()) {
                    req.setRawHeader("Authorization", "Bearer " + _cred->password().toUtf8());
                } else {
                    QByteArray credHash = QByteArray(_cred->user().toUtf8() + ":" + _cred->password().toUtf8()).toBase64();
                    req.setRawHeader("Authorization", "Basic " + credHash);
                }
            } else if (!request.url().password().isEmpty()) {
                // Typically the requests to get or refresh the OAuth access token. The client
                // credentials are put in the URL from the code making the request.
                QByteArray credHash = request.url().userInfo().toUtf8().toBase64();
                req.setRawHeader("Authorization", "Basic " + credHash);
            }
        }

        if (_cred && !_cred->_clientSslKey.isNull() && !_cred->_clientSslCertificate.isNull()) {
            // SSL configuration
            QSslConfiguration sslConfiguration = req.sslConfiguration();
            sslConfiguration.setLocalCertificate(_cred->_clientSslCertificate);
            sslConfiguration.setPrivateKey(_cred->_clientSslKey);
            req.setSslConfiguration(sslConfiguration);
        }


        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    // The credentials object dies along with the account, while the QNAM might
    // outlive both.
    QPointer<const HttpCredentials> _cred;
};


static void addSettingsToJob(Account *account, QKeychain::Job *job)
{
    Q_UNUSED(account);
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings.release());
}

HttpCredentials::HttpCredentials()
    : _ready(false)
    , _keychainMigration(false)
{
}

// From wizard
HttpCredentials::HttpCredentials(const QString &user, const QString &password, const QSslCertificate &certificate, const QSslKey &key)
    : _user(user)
    , _password(password)
    , _ready(true)
    , _clientSslKey(key)
    , _clientSslCertificate(certificate)
    , _keychainMigration(false)
    , _retryOnKeyChainError(false)
{
}

QString HttpCredentials::authType() const
{
    return QString::fromLatin1("http");
}

QString HttpCredentials::user() const
{
    return _user;
}

QString HttpCredentials::password() const
{
    return _password;
}

void HttpCredentials::setAccount(Account *account)
{
    AbstractCredentials::setAccount(account);
    if (_user.isEmpty()) {
        fetchUser();
    }
}

QNetworkAccessManager *HttpCredentials::createQNAM() const
{
    AccessManager *qnam = new HttpCredentialsAccessManager(this);

    connect(qnam, &QNetworkAccessManager::authenticationRequired,
        this, &HttpCredentials::slotAuthentication);

    return qnam;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

QString HttpCredentials::fetchUser()
{
    _user = _account->credentialSetting(QLatin1String(userC)).toString();
    return _user;
}

void HttpCredentials::fetchFromKeychain()
{
    _wasFetched = true;

    // User must be fetched from config file
    fetchUser();

    if (!_ready && !_refreshToken.isEmpty()) {
        // This happens if the credentials are still loaded from the keychain, bur we are called
        // here because the auth is invalid, so this means we simply need to refresh the credentials
        refreshAccessToken();
        return;
    }

    if (_ready) {
        Q_EMIT fetched();
    } else {
        _keychainMigration = false;
        fetchFromKeychainHelper();
    }
}

void HttpCredentials::fetchFromKeychainHelper()
{
    // Read client cert from keychain
    const QString kck = keychainKey(
        _account->url().toString(),
        _user + clientCertificatePEMC,
        _keychainMigration ? QString() : _account->id());

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &Job::finished, this, &HttpCredentials::slotReadClientCertPEMJobDone);
    job->start();
}

void HttpCredentials::deleteOldKeychainEntries()
{
    auto startDeleteJob = [this](QString user) {
        DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(true);
        job->setKey(keychainKey(_account->url().toString(), user, QString()));
        job->start();
    };

    startDeleteJob(_user);
    startDeleteJob(_user + clientKeyPEMC);
    startDeleteJob(_user + clientCertificatePEMC);
}

void HttpCredentials::slotReadClientCertPEMJobDone(QKeychain::Job *incoming)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    Q_ASSERT(!incoming->insecureFallback()); // If insecureFallback is set, the next test would be pointless
    if (_retryOnKeyChainError && (incoming->error() == QKeychain::NoBackendAvailable
            || incoming->error() == QKeychain::OtherError)) {
        // Could be that the backend was not yet available. Wait some extra seconds.
        // (Issues #4274 and #6522)
        // (For kwallet, the error is OtherError instead of NoBackendAvailable, maybe a bug in QtKeychain)
        qCInfo(lcHttpCredentials) << "Backend unavailable (yet?) Retrying in a few seconds." << incoming->errorString();
        QTimer::singleShot(10000, this, &HttpCredentials::fetchFromKeychainHelper);
        _retryOnKeyChainError = false;
        return;
    }
    _retryOnKeyChainError = false;
#endif

    // Store PEM in memory
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incoming);
    if (readJob->error() == NoError && readJob->binaryData().length() > 0) {
        QList<QSslCertificate> sslCertificateList = QSslCertificate::fromData(readJob->binaryData(), QSsl::Pem);
        if (sslCertificateList.length() >= 1) {
            _clientSslCertificate = sslCertificateList.at(0);
        }
    }

    // Load key too
    const QString kck = keychainKey(
        _account->url().toString(),
        _user + clientKeyPEMC,
        _keychainMigration ? QString() : _account->id());

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &Job::finished, this, &HttpCredentials::slotReadClientKeyPEMJobDone);
    job->start();
}

void HttpCredentials::slotReadClientKeyPEMJobDone(QKeychain::Job *incoming)
{
    // Store key in memory
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incoming);

    if (readJob->error() == NoError && readJob->binaryData().length() > 0) {
        QByteArray clientKeyPEM = readJob->binaryData();
        // FIXME Unfortunately Qt has a bug and we can't just use QSsl::Opaque to let it
        // load whatever we have. So we try until it works.
        _clientSslKey = QSslKey(clientKeyPEM, QSsl::Rsa);
        if (_clientSslKey.isNull()) {
            _clientSslKey = QSslKey(clientKeyPEM, QSsl::Dsa);
        }
        if (_clientSslKey.isNull()) {
            _clientSslKey = QSslKey(clientKeyPEM, QSsl::Ec);
        }
        if (_clientSslKey.isNull()) {
            qCWarning(lcHttpCredentials) << "Could not load SSL key into Qt!";
        }
    }

    // Now fetch the actual server password
    const QString kck = keychainKey(
        _account->url().toString(),
        _user,
        _keychainMigration ? QString() : _account->id());

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &Job::finished, this, &HttpCredentials::slotReadJobDone);
    job->start();
}


bool HttpCredentials::stillValid(QNetworkReply *reply)
{
    return ((reply->error() != QNetworkReply::AuthenticationRequiredError)
        // returned if user or password is incorrect
        && (reply->error() != QNetworkReply::OperationCanceledError
               || !reply->property(authenticationFailedC).toBool()));
}

void HttpCredentials::slotReadJobDone(QKeychain::Job *incomingJob)
{
    QKeychain::ReadPasswordJob *job = static_cast<ReadPasswordJob *>(incomingJob);
    QKeychain::Error error = job->error();

    // If we can't find the credentials at the keys that include the account id,
    // try to read them from the legacy locations that don't have a account id.
    if (!_keychainMigration && error == QKeychain::EntryNotFound) {
        qCWarning(lcHttpCredentials)
            << "Could not find keychain entries, attempting to read from legacy locations";
        _keychainMigration = true;
        fetchFromKeychainHelper();
        return;
    }

    bool isOauth = _account->credentialSetting(QLatin1String(isOAuthC)).toBool();
    if (isOauth) {
        _refreshToken = job->textData();
    } else {
        _password = job->textData();
    }

    if (_user.isEmpty()) {
        qCWarning(lcHttpCredentials) << "Strange: User is empty!";
    }

    if (!_refreshToken.isEmpty() && error == NoError) {
        refreshAccessToken();
    } else if (!_password.isEmpty() && error == NoError) {
        // All cool, the keychain did not come back with error.
        // Still, the password can be empty which indicates a problem and
        // the password dialog has to be opened.
        _ready = true;
        emit fetched();
    } else {
        // we come here if the password is empty or any other keychain
        // error happend.

        _fetchErrorString = job->error() != EntryNotFound ? job->errorString() : QString();

        _password = QString();
        _ready = false;
        emit fetched();
    }

    // If keychain data was read from legacy location, wipe these entries and store new ones
    if (_keychainMigration && _ready) {
        persist();
        deleteOldKeychainEntries();
        qCWarning(lcHttpCredentials) << "Migrated old keychain entries";
    }
}

bool HttpCredentials::refreshAccessToken()
{
    if (_refreshToken.isEmpty())
        return false;

    QUrl requestToken = Utility::concatUrlPath(_account->url(), QLatin1String("/index.php/apps/oauth2/api/v1/token"));
    QNetworkRequest req;
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QString basicAuth = QString("%1:%2").arg(
        Theme::instance()->oauthClientId(), Theme::instance()->oauthClientSecret());
    req.setRawHeader("Authorization", "Basic " + basicAuth.toUtf8().toBase64());

    auto requestBody = new QBuffer;
    QUrlQuery arguments(QString("grant_type=refresh_token&refresh_token=%1").arg(_refreshToken));
    requestBody->setData(arguments.query(QUrl::FullyEncoded).toLatin1());

    auto job = _account->sendRequest("POST", requestToken, req, requestBody);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
    QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        auto jsonData = reply->readAll();
        QJsonParseError jsonParseError;
        QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();
        QString accessToken = json["access_token"].toString();
        if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError || json.isEmpty()) {
            // Network error maybe?
            qCWarning(lcHttpCredentials) << "Error while refreshing the token" << reply->errorString() << jsonData << jsonParseError.errorString();
        } else if (accessToken.isEmpty()) {
            // The token is no longer valid.
            qCDebug(lcHttpCredentials) << "Expired refresh token. Logging out";
            _refreshToken.clear();
        } else {
            _ready = true;
            _password = accessToken;
            _refreshToken = json["refresh_token"].toString();
            persist();
        }
        emit fetched();
    });
    return true;
}


void HttpCredentials::invalidateToken()
{
    if (!_password.isEmpty()) {
        _previousPassword = _password;
    }
    _password = QString();
    _ready = false;

    // User must be fetched from config file to generate a valid key
    fetchUser();

    const QString kck = keychainKey(_account->url().toString(), _user, _account->id());
    if (kck.isEmpty()) {
        qCWarning(lcHttpCredentials) << "InvalidateToken: User is empty, bailing out!";
        return;
    }

    // clear the session cookie.
    _account->clearCookieJar();

    if (!_refreshToken.isEmpty()) {
        // Only invalidate the access_token (_password) but keep the _refreshToken in the keychain
        // (when coming from forgetSensitiveData, the _refreshToken is cleared)
        return;
    }

    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(true);
    job->setKey(kck);
    job->start();

    // let QNAM forget about the password
    // This needs to be done later in the event loop because we might be called (directly or
    // indirectly) from QNetworkAccessManagerPrivate::authenticationRequired, which itself
    // is a called from a BlockingQueuedConnection from the Qt HTTP thread. And clearing the
    // cache needs to synchronize again with the HTTP thread.
    QTimer::singleShot(0, _account, &Account::clearQNAMCache);
}

void HttpCredentials::forgetSensitiveData()
{
    // need to be done before invalidateToken, so it actually deletes the refresh_token from the keychain
    _refreshToken.clear();

    invalidateToken();
    _previousPassword.clear();
}

void HttpCredentials::persist()
{
    if (_user.isEmpty()) {
        // We never connected or fetched the user, there is nothing to save.
        return;
    }

    _account->setCredentialSetting(QLatin1String(userC), _user);
    _account->setCredentialSetting(QLatin1String(isOAuthC), isUsingOAuth());
    _account->wantsAccountSaved(_account);

    // write cert if there is one
    if (!_clientSslCertificate.isNull()) {
        WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(false);
        connect(job, &Job::finished, this, &HttpCredentials::slotWriteClientCertPEMJobDone);
        job->setKey(keychainKey(_account->url().toString(), _user + clientCertificatePEMC, _account->id()));
        job->setBinaryData(_clientSslCertificate.toPem());
        job->start();
    } else {
        slotWriteClientCertPEMJobDone();
    }
}

void HttpCredentials::slotWriteClientCertPEMJobDone()
{
    // write ssl key if there is one
    if (!_clientSslKey.isNull()) {
        WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(false);
        connect(job, &Job::finished, this, &HttpCredentials::slotWriteClientKeyPEMJobDone);
        job->setKey(keychainKey(_account->url().toString(), _user + clientKeyPEMC, _account->id()));
        job->setBinaryData(_clientSslKey.toPem());
        job->start();
    } else {
        slotWriteClientKeyPEMJobDone();
    }
}

void HttpCredentials::slotWriteClientKeyPEMJobDone()
{
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    connect(job, &Job::finished, this, &HttpCredentials::slotWriteJobDone);
    job->setKey(keychainKey(_account->url().toString(), _user, _account->id()));
    job->setTextData(isUsingOAuth() ? _refreshToken : _password);
    job->start();
}

void HttpCredentials::slotWriteJobDone(QKeychain::Job *job)
{
    delete job->settings();
    switch (job->error()) {
    case NoError:
        break;
    default:
        qCWarning(lcHttpCredentials) << "Error while writing password" << job->errorString();
    }
    WritePasswordJob *wjob = qobject_cast<WritePasswordJob *>(job);
    wjob->deleteLater();
}

void HttpCredentials::slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (!_ready)
        return;
    Q_UNUSED(authenticator)
    // Because of issue #4326, we need to set the login and password manually at every requests
    // Thus, if we reach this signal, those credentials were invalid and we terminate.
    qCWarning(lcHttpCredentials) << "Stop request: Authentication failed for " << reply->url().toString();
    reply->setProperty(authenticationFailedC, true);
    reply->close();
}

} // namespace OCC
