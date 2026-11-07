#include "httpserver.h"
#include "exportedvideomanager.h"
#include <QHttpServerResponse>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpHeaders>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QMimeDatabase>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QLocale>
#include <QDebug>
#include <algorithm>

namespace {

// A read-only QIODevice that exposes a [start, start + length) slice of a file.
//
// QHttpServerResponder::write(QIODevice *, ...) streams such a device to the
// client in fixed-size chunks (instead of reading the whole file into memory)
// and takes the Content-Length header of the response from size(), so size()
// must report the length of the slice, not of the underlying file.
class FileSegmentDevice : public QIODevice
{
public:
    FileSegmentDevice(const QString &filePath, qint64 start, qint64 length)
        : m_file(filePath), m_start(start), m_length(length)
    {
    }

    ~FileSegmentDevice() override
    {
        close();
    }

    bool open(OpenMode mode) override
    {
        if (!m_file.open(QIODevice::ReadOnly))
            return false;
        if (!m_file.seek(m_start)) {
            m_file.close();
            return false;
        }
        m_remaining = m_length;
        QIODevice::open(mode | QIODevice::ReadOnly);
        return true;
    }

    void close() override
    {
        m_file.close();
        QIODevice::close();
    }

    bool isSequential() const override { return false; }
    qint64 size() const override { return m_length; }
    bool atEnd() const override { return !m_file.isOpen() || m_remaining == 0; }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (!m_file.isOpen() || m_remaining <= 0)
            return 0;
        const qint64 toRead = qMin(maxSize, m_remaining);
        const qint64 bytesRead = m_file.read(data, toRead);
        if (bytesRead > 0)
            m_remaining -= bytesRead;
        return bytesRead;
    }

    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

private:
    QFile m_file;
    const qint64 m_start;
    const qint64 m_length;
    qint64 m_remaining = 0;
};

struct ByteRangeSpec {
    enum class Status {
        Invalid,       // syntactically broken; the Range header must be ignored (RFC 9110, 14.2)
        Unsatisfiable, // syntactically valid, but outside the current representation
        Ok,
    };
    Status status = Status::Invalid;
    qint64 start = 0;
    qint64 end = 0; // inclusive
};

// Parses the first range of a "Range:" header value against a representation
// of fileSize bytes. Multi-part ranges are not supported; if the client asks
// for several ranges the first one is served (browsers never request
// multi-part downloads).
ByteRangeSpec parseByteRange(const QByteArray &rangeHeader, qint64 fileSize)
{
    ByteRangeSpec spec;
    const QString header = QString::fromLatin1(rangeHeader).trimmed();
    if (!header.startsWith(QLatin1String("bytes="), Qt::CaseInsensitive))
        return spec;

    const QString firstSpec = header.mid(6).split(',').first().trimmed();
    const int dashPos = firstSpec.indexOf('-');
    if (dashPos < 0)
        return spec;

    const QString startStr = firstSpec.left(dashPos).trimmed();
    const QString endStr = firstSpec.mid(dashPos + 1).trimmed();

    const auto allDigits = [](const QString &s) {
        return !s.isEmpty()
               && std::all_of(s.cbegin(), s.cend(), [](QChar c) { return c.isDigit(); });
    };

    bool ok = false;
    if (startStr.isEmpty()) {
        // Suffix range "-N": the last N bytes of the file.
        if (!allDigits(endStr))
            return spec;
        const qint64 n = endStr.toLongLong(&ok);
        if (!ok || n <= 0 || fileSize <= 0) {
            spec.status = ByteRangeSpec::Status::Unsatisfiable;
            return spec;
        }
        spec.start = qMax<qint64>(0, fileSize - n);
        spec.end = fileSize - 1;
    } else {
        if (!allDigits(startStr))
            return spec;
        spec.start = startStr.toLongLong(&ok);
        if (!ok)
            return spec;
        if (endStr.isEmpty()) {
            spec.end = fileSize - 1;
        } else {
            if (!allDigits(endStr))
                return spec;
            spec.end = endStr.toLongLong(&ok);
            if (!ok)
                return spec;
            if (spec.end < spec.start)
                return spec; // syntactically invalid, ignore the whole header
            spec.end = qMin(spec.end, fileSize - 1);
        }
        if (fileSize <= 0 || spec.start >= fileSize) {
            spec.status = ByteRangeSpec::Status::Unsatisfiable;
            return spec;
        }
    }
    spec.status = ByteRangeSpec::Status::Ok;
    return spec;
}

} // namespace


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
    
    // Download route - serve video files.
    //
    // This route must use the QHttpServerResponder-based (streaming) form
    // instead of returning a QHttpServerResponse: responses built from a
    // QByteArray are copied into the socket's write buffer in one go, after
    // which QHttpServer considers the connection idle again. The keep-alive
    // watchdog of QHttpServer (Qt 6.10+) then aborts connections whose last
    // request is older than the keep-alive timeout (15s by default), which
    // kills transfers of large videos half-way through. Streaming a QIODevice
    // keeps the transfer state active until the last byte is written.
    m_server->route("/download/<arg>", [this](const QString &fileName,
                                              const QHttpServerRequest &request,
                                              QHttpServerResponder &&responder) {
        incrementConnectionCount();
        handleVideoDownload(fileName, request, std::move(responder));
        decrementConnectionCount();
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
    // The page always comes from the compiled-in resource; there is no
    // generated fallback.
    QFile resourceFile(":/web/index.html");
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open :/web/index.html from resources";
        return QHttpServerResponse(generateErrorHtml("Web page resource is not available").toUtf8(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
    }

    QByteArray content = resourceFile.readAll();
    QHttpServerResponse response(content);

    // Set content type header
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
            videoObj["fileSize"] = video->fileSizeString();
            videoObj["fileSizeBytes"] = video->fileSize();
            videoObj["creationTime"] = video->creationTimeString();
            videoObj["creationTimeMs"] = video->creationTime().toMSecsSinceEpoch();
            videoObj["downloadUrl"] = QString("/download/%1").arg(video->fileName());
            videos.append(videoObj);
        }
    }
    
    QJsonObject response;
    response["videos"] = videos;
    response["count"] = videos.size();
    response["totalSize"] = m_videoManager->totalSizeString();
    
    QHttpServerResponse httpResponse(QJsonDocument(response).toJson());
    
    QHttpHeaders headers = httpResponse.headers();
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, "application/json");
    httpResponse.setHeaders(std::move(headers));
    
    return httpResponse;
}

void HttpServer::handleVideoDownload(const QString &fileName, const QHttpServerRequest &request,
                                     QHttpServerResponder &&responder)
{
    const auto writeHtmlError = [&responder](const QString &html,
                                             QHttpServerResponder::StatusCode status) {
        QHttpHeaders headers;
        headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/html; charset=utf-8");
        responder.write(html.toUtf8(), headers, status);
    };

    if (!m_videoManager) {
        writeHtmlError(generateErrorHtml("Video manager not available"),
                       QHttpServerResponder::StatusCode::InternalServerError);
        return;
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
        writeHtmlError(generateNotFoundHtml(), QHttpServerResponder::StatusCode::NotFound);
        return;
    }

    // Check if file exists
    QFileInfo fileInfo(targetVideo->filePath());
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        writeHtmlError(generateErrorHtml("File not found or not readable"),
                       QHttpServerResponder::StatusCode::NotFound);
        return;
    }

    const qint64 fileSize = fileInfo.size();
    const QString contentType = getContentType(targetVideo->filePath());

    // Common headers. Content-Length is derived from the streamed device by
    // the responder itself, so it must not be set here (that would produce a
    // duplicated Content-Length header).
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, contentType.toUtf8());
    headers.append("Accept-Ranges", "bytes");
    // Remove attachment disposition for video preview
    if (!contentType.startsWith("video/")) {
        headers.append(QHttpHeaders::WellKnownHeader::ContentDisposition,
                       QString("attachment; filename=\"%1\"").arg(fileName).toUtf8());
    }

    // Check for Range request
    const QByteArray rangeHeader = request.value("Range");
    const ByteRangeSpec range = parseByteRange(rangeHeader, fileSize);

    if (!rangeHeader.isEmpty() && range.status == ByteRangeSpec::Status::Unsatisfiable) {
        QHttpHeaders rangeHeaders;
        rangeHeaders.append("Content-Range", QString("bytes */%1").arg(fileSize).toUtf8());
        responder.write(rangeHeaders,
                        QHttpServerResponder::StatusCode::RequestRangeNotSatisfiable);
        return;
    }

    // Update statistics
    m_totalDownloads++;
    emit fileDownloaded(fileName, request.remoteAddress().toString());

    if (!rangeHeader.isEmpty() && range.status == ByteRangeSpec::Status::Ok) {
        // Partial content: stream [range.start, range.end] from disk in
        // chunks. Resumable downloads (e.g. Chrome retrying an interrupted
        // transfer) land here, so the response must be a proper 206 with a
        // matching Content-Range header.
        const qint64 contentLength = range.end - range.start + 1;
        headers.append("Content-Range", QString("bytes %1-%2/%3")
                                            .arg(range.start)
                                            .arg(range.end)
                                            .arg(fileSize)
                                            .toUtf8());
        m_totalBytesServed += contentLength;
        responder.write(new FileSegmentDevice(targetVideo->filePath(), range.start, contentLength),
                        headers, QHttpServerResponder::StatusCode::PartialContent);
        return;
    }

    // Full file download. A syntactically invalid Range header is ignored,
    // as allowed by RFC 9110.
    m_totalBytesServed += fileSize;
    responder.write(new QFile(targetVideo->filePath()), headers,
                    QHttpServerResponder::StatusCode::Ok);
}

QHttpServerResponse HttpServer::handleStaticFiles(const QString &path)
{
    // For future use - serve CSS/JS/images
    Q_UNUSED(path)
    return QHttpServerResponse(generateNotFoundHtml().toUtf8(),
                             QHttpServerResponse::StatusCode::NotFound);
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
