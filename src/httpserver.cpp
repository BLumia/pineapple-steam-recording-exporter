#include "httpserver.h"
#include "exportedvideomanager.h"
#include <QHttpServerResponse>
#include <QHttpServerRequest>
#include <QHttpHeaders>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QMimeDatabase>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QLocale>
#include <QDebug>

HttpServer::HttpServer(QObject *parent)
    : QObject(parent)
    , m_server(new QHttpServer(this))
    , m_tcpServer(new QTcpServer(this))
    , m_videoManager(nullptr)
    , m_isRunning(false)
    , m_port(6487)
    , m_connectionCount(0)
    , m_totalDownloads(0)
    , m_totalBytesServed(0)
{
    setupRoutes();
    updateServerUrl();
}

HttpServer::~HttpServer()
{
    stopServer();
}

void HttpServer::setPort(int port)
{
    if (m_port == port) {
        return;
    }
    
    bool wasRunning = m_isRunning;
    if (wasRunning) {
        stopServer();
    }
    
    m_port = port;
    updateServerUrl();
    emit portChanged();
    
    if (wasRunning) {
        startServer();
    }
}

void HttpServer::setExportedVideoManager(ExportedVideoManager *manager)
{
    if (m_videoManager == manager) {
        return;
    }
    
    if (m_videoManager) {
        disconnect(m_videoManager, nullptr, this, nullptr);
    }
    
    m_videoManager = manager;
    
    if (m_videoManager) {
        connect(m_videoManager, &ExportedVideoManager::videoCountChanged,
                this, &HttpServer::onVideoManagerChanged);
        connect(m_videoManager, &ExportedVideoManager::exportDirectoryChanged,
                this, &HttpServer::onVideoManagerChanged);
    }
}

bool HttpServer::startServer()
{
    if (m_isRunning) {
        return true;
    }
    
    setLastError("");
    
    // First start the TCP server
    if (!m_tcpServer->listen(QHostAddress::Any, m_port)) {
        setLastError(QString("Failed to start TCP server on port %1: %2")
                     .arg(m_port).arg(m_tcpServer->errorString()));
        emit serverError(m_lastError);
        return false;
    }
    
    // Then bind the HTTP server to the TCP server
    if (!m_server->bind(m_tcpServer)) {
        setLastError(QString("Failed to bind HTTP server to TCP server"));
        m_tcpServer->close();
        emit serverError(m_lastError);
        return false;
    }
    
    // Update the actual port (in case we requested 0 and got a random port)
    m_port = m_tcpServer->serverPort();
    
    setIsRunning(true);
    updateServerUrl();
    emit serverStarted();
    
    qDebug() << "HTTP server started on port" << m_port;
    qDebug() << "Server URL:" << m_serverUrl;
    
    return true;
}

void HttpServer::stopServer()
{
    if (!m_isRunning) {
        return;
    }
    
    m_tcpServer->close();
    setIsRunning(false);
    m_connectionCount = 0;
    emit connectionCountChanged();
    emit serverStopped();
    
    qDebug() << "HTTP server stopped";
}

void HttpServer::restartServer()
{
    stopServer();
    startServer();
}

void HttpServer::setupRoutes()
{
    // Root route - serve video list HTML
    m_server->route("/", [this]() {
        incrementConnectionCount();
        auto response = handleRootRequest();
        decrementConnectionCount();
        return response;
    });
    
    // API route - return video list as JSON
    m_server->route("/api/videos", [this]() {
        incrementConnectionCount();
        auto response = handleVideoListApi();
        decrementConnectionCount();
        return response;
    });
    
    // Download route - serve video files
    m_server->route("/download/<arg>", [this](const QString &fileName) {
        incrementConnectionCount();
        auto response = handleVideoDownload(fileName);
        decrementConnectionCount();
        return response;
    });
    
    // Static files route for potential CSS/JS assets
    m_server->route("/static/<arg>", [this](const QString &path) {
        incrementConnectionCount();
        auto response = handleStaticFiles(path);
        decrementConnectionCount();
        return response;
    });
}

QHttpServerResponse HttpServer::handleRootRequest()
{
    if (!m_videoManager) {
        return QHttpServerResponse(generateErrorHtml("Video manager not available").toUtf8(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
    }
    
    QString html = generateVideoListHtml();
    QHttpServerResponse response(html.toUtf8());
    
    // Set content type header using Qt 6.8+ API
    QHttpHeaders headers = response.headers();
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/html; charset=utf-8");
    response.setHeaders(std::move(headers));
    
    return response;
}

QHttpServerResponse HttpServer::handleVideoListApi()
{
    if (!m_videoManager) {
        QJsonObject error;
        error["error"] = "Video manager not available";
        QHttpServerResponse response(QJsonDocument(error).toJson(),
                                   QHttpServerResponse::StatusCode::InternalServerError);
        
        QHttpHeaders headers = response.headers();
        headers.append(QHttpHeaders::WellKnownHeader::ContentType, "application/json");
        response.setHeaders(std::move(headers));
        
        return response;
    }
    
    QJsonArray videos;
    for (int i = 0; i < m_videoManager->videoCount(); ++i) {
        ExportedVideo *video = m_videoManager->getVideo(i);
        if (video) {
            QJsonObject videoObj;
            videoObj["fileName"] = video->fileName();
            videoObj["displayName"] = video->displayName();
            videoObj["fileSize"] = video->fileSize();
            videoObj["fileSizeString"] = video->fileSizeString();
            videoObj["creationTime"] = video->creationTime().toString(Qt::ISODate);
            videoObj["creationTimeString"] = video->creationTimeString();
            videoObj["downloadUrl"] = QString("/download/%1").arg(video->fileName());
            videos.append(videoObj);
        }
    }
    
    QJsonObject response;
    response["videos"] = videos;
    response["totalCount"] = videos.size();
    response["totalSize"] = m_videoManager->totalSize();
    response["totalSizeString"] = m_videoManager->totalSizeString();
    
    QHttpServerResponse httpResponse(QJsonDocument(response).toJson());
    
    QHttpHeaders headers = httpResponse.headers();
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, "application/json");
    httpResponse.setHeaders(std::move(headers));
    
    return httpResponse;
}

QHttpServerResponse HttpServer::handleVideoDownload(const QString &fileName)
{
    if (!m_videoManager) {
        return QHttpServerResponse(generateErrorHtml("Video manager not available").toUtf8(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
    }
    
    // Find the video file
    ExportedVideo *targetVideo = nullptr;
    for (int i = 0; i < m_videoManager->videoCount(); ++i) {
        ExportedVideo *video = m_videoManager->getVideo(i);
        if (video && video->fileName() == fileName) {
            targetVideo = video;
            break;
        }
    }
    
    if (!targetVideo) {
        return QHttpServerResponse(generateNotFoundHtml().toUtf8(),
                                 QHttpServerResponse::StatusCode::NotFound);
    }
    
    // Check if file exists
    QFileInfo fileInfo(targetVideo->filePath());
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        return QHttpServerResponse(generateErrorHtml("File not found or not readable").toUtf8(),
                                 QHttpServerResponse::StatusCode::NotFound);
    }
    
    // Read file content
    QFile file(targetVideo->filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return QHttpServerResponse(generateErrorHtml("Failed to read file").toUtf8(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
    }
    
    QByteArray fileContent = file.readAll();
    file.close();
    
    // Update statistics
    m_totalDownloads++;
    m_totalBytesServed += fileContent.size();
    emit fileDownloaded(fileName, "unknown"); // TODO: Get actual client IP
    
    // Determine content type
    QString contentType = getContentType(targetVideo->filePath());
    
    // Create response with file content
    QHttpServerResponse response(fileContent);
    
    // Set proper headers using Qt 6.8+ API
    QHttpHeaders headers = response.headers();
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, contentType.toUtf8());
    headers.append(QHttpHeaders::WellKnownHeader::ContentDisposition,
                  QString("attachment; filename=\"%1\"").arg(fileName).toUtf8());
    headers.append(QHttpHeaders::WellKnownHeader::ContentLength,
                  QString::number(fileContent.size()).toUtf8());
    response.setHeaders(std::move(headers));
    
    return response;
}

QHttpServerResponse HttpServer::handleStaticFiles(const QString &path)
{
    // For future use - serve CSS/JS/images
    Q_UNUSED(path)
    return QHttpServerResponse(generateNotFoundHtml().toUtf8(),
                             QHttpServerResponse::StatusCode::NotFound);
}

QString HttpServer::generateVideoListHtml()
{
    QString html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Pineapple Steam Recording Exports</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
            color: #333;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 {
            margin: 0;
            font-size: 2.5em;
            font-weight: 300;
        }
        .stats {
            display: flex;
            justify-content: center;
            gap: 40px;
            margin-top: 20px;
        }
        .stat {
            text-align: center;
        }
        .stat-value {
            font-size: 1.5em;
            font-weight: bold;
        }
        .stat-label {
            opacity: 0.9;
            font-size: 0.9em;
        }
        .content {
            padding: 30px;
        }
        .video-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        .video-card {
            border: 1px solid #ddd;
            border-radius: 8px;
            overflow: hidden;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        .video-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
        }
        .video-info {
            padding: 20px;
        }
        .video-title {
            font-size: 1.2em;
            font-weight: 600;
            margin: 0 0 10px 0;
            color: #333;
        }
        .video-details {
            color: #666;
            font-size: 0.9em;
            margin: 5px 0;
        }
        .download-btn {
            display: inline-block;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            text-decoration: none;
            padding: 12px 24px;
            border-radius: 6px;
            margin-top: 15px;
            transition: opacity 0.2s;
            font-weight: 500;
        }
        .download-btn:hover {
            opacity: 0.9;
        }
        .empty-state {
            text-align: center;
            padding: 60px 20px;
            color: #666;
        }
        .empty-icon {
            font-size: 4em;
            margin-bottom: 20px;
            opacity: 0.3;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🍍 Steam Recording Exports</h1>
            <div class="stats">
                <div class="stat">
                    <div class="stat-value">%1</div>
                    <div class="stat-label">Videos</div>
                </div>
                <div class="stat">
                    <div class="stat-value">%2</div>
                    <div class="stat-label">Total Size</div>
                </div>
            </div>
        </div>
        <div class="content">
            %3
        </div>
    </div>
</body>
</html>
)";

    if (!m_videoManager || m_videoManager->videoCount() == 0) {
        QString emptyContent = R"(
            <div class="empty-state">
                <div class="empty-icon">📹</div>
                <h2>No Videos Available</h2>
                <p>No exported videos found. Export some recordings first!</p>
            </div>
        )";
        return html.arg("0").arg("0 B").arg(emptyContent);
    }

    QString videoCards = "<div class=\"video-grid\">";
    for (int i = 0; i < m_videoManager->videoCount(); ++i) {
        ExportedVideo *video = m_videoManager->getVideo(i);
        if (video) {
            QString card = R"(
                <div class="video-card">
                    <div class="video-info">
                        <h3 class="video-title">%1</h3>
                        <div class="video-details">📁 %2</div>
                        <div class="video-details">📏 %3</div>
                        <div class="video-details">📅 %4</div>
                        <a href="/download/%5" class="download-btn">⬇️ Download</a>
                    </div>
                </div>
            )";
            videoCards += card.arg(video->displayName())
                             .arg(video->fileName())
                             .arg(video->fileSizeString())
                             .arg(video->creationTimeString())
                             .arg(video->fileName());
        }
    }
    videoCards += "</div>";

    return html.arg(QString::number(m_videoManager->videoCount()))
               .arg(m_videoManager->totalSizeString())
               .arg(videoCards);
}

QString HttpServer::generateNotFoundHtml()
{
    return R"(
<!DOCTYPE html>
<html><head><title>404 Not Found</title></head>
<body>
<h1>404 - Not Found</h1>
<p>The requested resource was not found on this server.</p>
</body></html>
)";
}

QString HttpServer::generateErrorHtml(const QString &error)
{
    return QString(R"(
<!DOCTYPE html>
<html><head><title>Error</title></head>
<body>
<h1>Server Error</h1>
<p>%1</p>
</body></html>
)").arg(error);
}

QString HttpServer::getContentType(const QString &filePath)
{
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    return mimeType.name();
}

void HttpServer::updateServerUrl()
{
    // Get the first available network interface IP
    QString hostAddress = "localhost";
    
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && 
            !address.isLoopback() && !address.isMulticast()) {
            hostAddress = address.toString();
            break;
        }
    }
    
    m_serverUrl = QString("http://%1:%2").arg(hostAddress).arg(m_port);
    emit serverUrlChanged();
}

void HttpServer::setIsRunning(bool running)
{
    if (m_isRunning == running) {
        return;
    }
    
    m_isRunning = running;
    emit isRunningChanged();
}

void HttpServer::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    
    m_lastError = error;
    emit lastErrorChanged();
}

void HttpServer::incrementConnectionCount()
{
    m_connectionCount++;
    emit connectionCountChanged();
}

void HttpServer::decrementConnectionCount()
{
    if (m_connectionCount > 0) {
        m_connectionCount--;
        emit connectionCountChanged();
    }
}

void HttpServer::onVideoManagerChanged()
{
    // Video manager content changed, no specific action needed
    // The routes will automatically use the updated content
}
