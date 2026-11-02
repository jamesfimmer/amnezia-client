#include "providerController.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "core/controllers/selfhosted/importController.h"
#include "core/repositories/secureServersRepository.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/serverConfigUtils.h"
#include "secureQSettings.h"
#include "version.h"

#ifndef PROVIDER_API_URL
    #define PROVIDER_API_URL ""
#endif

namespace
{
    const QString urlSettingKey = QStringLiteral("Conf/providerUrl");
    const QString tokenSettingKey = QStringLiteral("Conf/providerToken");
    const QString mapSettingKey = QStringLiteral("Conf/providerServerMap");
    const QString deviceSettingKey = QStringLiteral("Conf/providerDeviceId");

    QString localIdForRemoteId(const QString &remoteId)
    {
        const QByteArray digest = QCryptographicHash::hash(remoteId.toUtf8(), QCryptographicHash::Sha1).toHex();
        return QStringLiteral("provider-") + QString::fromLatin1(digest.constData(), digest.size());
    }
}

ProviderController::ProviderController(SecureQSettings *settings, SecureServersRepository *serversRepository,
                                       ImportController *importController, QObject *parent)
    : QObject(parent), m_settings(settings), m_serversRepository(serversRepository), m_importController(importController)
{
    QTimer::singleShot(2000, this, [this]() {
        if (configured()) {
            refresh();
        }
    });
}

bool ProviderController::busy() const
{
    return m_busy;
}

bool ProviderController::configured() const
{
    return !endpoint().isEmpty();
}

QString ProviderController::status() const
{
    return m_status;
}

QString ProviderController::lastError() const
{
    return m_lastError;
}

void ProviderController::refresh()
{
    if (m_busy) {
        return;
    }

    const QString baseUrl = endpoint();
    if (baseUrl.isEmpty()) {
        setStatus(tr("Provider is not configured"));
        return;
    }

    const QUrl url(baseUrl + QStringLiteral("/v1/servers"));
    const bool localHttp = url.scheme() == QStringLiteral("http")
            && (url.host() == QStringLiteral("127.0.0.1") || url.host() == QStringLiteral("localhost")
                || url.host() == QStringLiteral("::1"));
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (url.scheme() != QStringLiteral("https") && !localHttp)) {
        setLastError(tr("Provider URL must use HTTPS (HTTP is allowed only on localhost)"));
        return;
    }

    setBusy(true);
    setLastError(QString());
    setStatus(tr("Refreshing server list"));

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("X-Device-Id", deviceId().toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(QStringLiteral(APPLICATION_NAME), QStringLiteral(APP_VERSION)));
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);

    qInfo() << "Provider: requesting server list from" << request.url().toString();

    const QString tokenValue = token();
    if (!tokenValue.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + tokenValue.toUtf8());
    }

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
}

void ProviderController::setEndpoint(const QString &endpointValue)
{
    QString normalized = endpointValue.trimmed();
    while (normalized.endsWith('/')) {
        normalized.chop(1);
    }
    m_settings->setValue(urlSettingKey, normalized);
    emit configuredChanged();
}

void ProviderController::setToken(const QString &tokenValue)
{
    m_settings->setValue(tokenSettingKey, tokenValue.trimmed());
}

void ProviderController::handleReply(QNetworkReply *reply)
{
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        setBusy(false);
        setLastError(networkErrorText);
        setStatus(tr("Server list request failed"));
        return;
    }

    if (statusCode >= 400) {
        setBusy(false);
        setLastError(QStringLiteral("HTTP %1").arg(statusCode));
        setStatus(tr("Server list request failed"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setBusy(false);
        setLastError(parseError.errorString());
        setStatus(tr("Invalid server list response"));
        return;
    }

    if (!document.isArray() && (!document.isObject() || !document.object().value(QStringLiteral("servers")).isArray())) {
        setBusy(false);
        setLastError(tr("Server list response must contain a servers array"));
        setStatus(tr("Invalid server list response"));
        return;
    }

    const QJsonArray servers =
            document.isObject() ? document.object().value(QStringLiteral("servers")).toArray() : document.array();

    const int applied = applyServers(servers);
    setBusy(false);
    setLastError(QString());
    setStatus(tr("Servers updated: %1").arg(applied));
    qInfo() << "Provider: imported" << applied << "of" << servers.size() << "servers";
    emit serversUpdated(applied);
}

int ProviderController::applyServers(const QJsonArray &servers)
{
    QJsonObject mapping = QJsonDocument::fromJson(mappingRaw().toUtf8()).object();
    int applied = 0;

    for (const QJsonValue &value : servers) {
        const QJsonObject item = value.toObject();
        const QString remoteId = item.value(QStringLiteral("id")).toString();
        const QString name = item.value(QStringLiteral("name")).toString();
        const QString configText = item.value(QStringLiteral("config")).toString();

        if (remoteId.isEmpty() || configText.isEmpty()) {
            continue;
        }

        const ImportController::ImportResult result = m_importController->extractConfigFromData(configText, name);
        if (result.errorCode != ErrorCode::NoError || result.config.isEmpty()
            || !serverConfigUtils::isConfigFormatVersionSupported(result.config)) {
            qWarning() << "Provider server" << remoteId << "has an invalid config";
            continue;
        }

        QJsonObject config = result.config;
        if (!name.isEmpty()) {
            config[configKey::description] = name;
            config[configKey::displayName] = name;
        }

        const auto kind = serverConfigUtils::configTypeFromJson(config);
        if (kind != serverConfigUtils::ConfigType::Native) {
            qWarning() << "Provider server" << remoteId << "has an unsupported config type";
            continue;
        }

        const QString mappedId = mapping.value(remoteId).toString();
        QString serverId;
        if (!mappedId.isEmpty() && m_serversRepository->indexOfServerId(mappedId) >= 0) {
            serverId = mappedId;
            m_serversRepository->editServer(serverId, config, kind);
        } else {
            const QString candidateId = localIdForRemoteId(remoteId);
            if (m_serversRepository->indexOfServerId(candidateId) >= 0) {
                serverId = candidateId;
                m_serversRepository->editServer(serverId, config, kind);
            } else {
                serverId = m_serversRepository->addServer(candidateId, config, kind);
            }
        }

        if (serverId.isEmpty() || m_serversRepository->indexOfServerId(serverId) < 0) {
            qWarning() << "Provider server" << remoteId << "was not stored";
            continue;
        }

        mapping[remoteId] = serverId;
        ++applied;
    }

    storeMapping(QString::fromUtf8(QJsonDocument(mapping).toJson(QJsonDocument::Compact)));
    return applied;
}

QString ProviderController::endpoint() const
{
    QString result;
    const QByteArray environmentUrl = qgetenv("PROVIDER_API_URL");
    if (!environmentUrl.isEmpty()) {
        result = QString::fromUtf8(environmentUrl).trimmed();
    } else {
        result = m_settings->value(urlSettingKey).toString().trimmed();
        if (result.isEmpty()) {
            result = QString::fromUtf8(PROVIDER_API_URL);
        }
    }

    while (result.endsWith('/')) {
        result.chop(1);
    }
    return result;
}

QString ProviderController::token() const
{
    return m_settings->value(tokenSettingKey).toString().trimmed();
}

QString ProviderController::deviceId()
{
    QString id = m_settings->value(deviceSettingKey).toString().trimmed();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings->setValue(deviceSettingKey, id);
    }
    return id;
}

QString ProviderController::mappingRaw() const
{
    return m_settings->value(mapSettingKey).toString();
}

void ProviderController::storeMapping(const QString &raw)
{
    m_settings->setValue(mapSettingKey, raw);
}

void ProviderController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void ProviderController::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void ProviderController::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}
