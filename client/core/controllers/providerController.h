#ifndef PROVIDERCONTROLLER_H
#define PROVIDERCONTROLLER_H

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class ImportController;
class QNetworkReply;
class SecureQSettings;
class SecureServersRepository;

class ProviderController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ProviderController(SecureQSettings *settings, SecureServersRepository *serversRepository,
                                ImportController *importController, QObject *parent = nullptr);

    bool busy() const;
    bool configured() const;
    QString status() const;
    QString lastError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEndpoint(const QString &endpoint);
    Q_INVOKABLE void setToken(const QString &token);

signals:
    void busyChanged();
    void configuredChanged();
    void statusChanged();
    void lastErrorChanged();
    void serversUpdated(int count);

private:
    void handleReply(QNetworkReply *reply);
    int applyServers(const QJsonArray &servers);

    QString endpoint() const;
    QString token() const;
    QString deviceId();
    QString mappingRaw() const;
    void storeMapping(const QString &raw);

    void setBusy(bool busy);
    void setStatus(const QString &status);
    void setLastError(const QString &error);

    SecureQSettings *m_settings = nullptr;
    SecureServersRepository *m_serversRepository = nullptr;
    ImportController *m_importController = nullptr;
    QNetworkAccessManager m_network;
    bool m_busy = false;
    QString m_status;
    QString m_lastError;
};

#endif // PROVIDERCONTROLLER_H
