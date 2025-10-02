#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QHttpServer>
#include <QHttpServerResponse>
#include <QHttpServerRequest>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

class ExportedVideoManager;
class QTcpServer;
class QHttpHeaders;

class HttpServer : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int connectionCount READ connectionCount NOTIFY connectionCountChanged)

public:
    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer() override;

    // Property getters
    bool isRunning() const { return m_isRunning; }
    int port() const { return m_port; }
    QString serverUrl() const { return m_serverUrl; }
    QString lastError() const { return m_lastError; }
    int connectionCount() const { return m_connectionCount; }

    // Property setters
    void setPort(int port);
    void setExportedVideoManager(ExportedVideoManager *manager);

    // Public methods
    Q_INVOKABLE bool startServer();
    Q_INVOKABLE void stopServer();
    Q_INVOKABLE void restartServer();

signals:
    void isRunningChanged();
    void portChanged();
    void serverUrlChanged();
    void lastErrorChanged();
    void connectionCountChanged();
    void serverStarted();
    void serverStopped();
    void serverError(const QString &error);
    void clientConnected(const QString &clientAddress);
    void clientDisconnected();
    void fileDownloaded(const QString &fileName, const QString &clientAddress);

private slots:
    void onVideoManagerChanged();

private:
    void setupRoutes();
    QHttpServerResponse handleRootRequest();
    QHttpServerResponse handleVideoListApi();
    QHttpServerResponse handleVideoDownload(const QString &fileName);
    QHttpServerResponse handleStaticFiles(const QString &path);
    
    QString generateVideoListHtml();
    QString generateNotFoundHtml();
    QString generateErrorHtml(const QString &error);
    QString getContentType(const QString &filePath);
    
    void updateServerUrl();
    void setIsRunning(bool running);
    void setLastError(const QString &error);
    void incrementConnectionCount();
    void decrementConnectionCount();

    QHttpServer *m_server;
    QTcpServer *m_tcpServer;
    ExportedVideoManager *m_videoManager;
    bool m_isRunning;
    int m_port;
    QString m_serverUrl;
    QString m_lastError;
    int m_connectionCount;
    
    // Server statistics
    qint64 m_totalDownloads;
    qint64 m_totalBytesServed;
};

#endif // HTTPSERVER_H