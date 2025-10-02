#include "exportedvideomanager.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDebug>
#include <QMimeDatabase>
#include <QMimeType>
#include <QCollator>
#include <QRegularExpression>
#include <QTimer>
#include <QFile>
#include <QLocale>
#include <algorithm>

// ExportedVideo implementation
ExportedVideo::ExportedVideo(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(filePath)
{
    updateFileInfo();
}

void ExportedVideo::updateFileInfo()
{
    QFileInfo fileInfo(m_filePath);
    
    m_fileName = fileInfo.fileName();
    m_displayName = extractDisplayName();
    m_fileSize = fileInfo.size();
    m_fileSizeString = QLocale().formattedDataSize(m_fileSize);
    m_creationTime = fileInfo.birthTime().isValid() ? fileInfo.birthTime() : fileInfo.lastModified();
    m_creationTimeString = m_creationTime.toString("yyyy-MM-dd hh:mm:ss");
    m_fileUrl = QUrl::fromLocalFile(m_filePath);
    
    // TODO: Generate thumbnail if needed
    m_thumbnailPath = "";
}



QString ExportedVideo::extractDisplayName() const
{
    QString baseName = QFileInfo(m_filePath).baseName();
    
    // Try to extract meaningful name from filename
    // Expected format: GameName_YYYYMMDD_HHMMSS or similar
    
    QString displayName = baseName;
    
    // Replace underscores with spaces
    displayName = displayName.replace('_', ' ');
    
    // Try to identify and format date/time patterns
    QRegularExpression dateTimePattern("(\\d{8})\\s+(\\d{6})");
    QRegularExpressionMatch match = dateTimePattern.match(displayName);
    if (match.hasMatch()) {
        QString dateStr = match.captured(1);
        QString timeStr = match.captured(2);
        
        // Format date as YYYY-MM-DD
        if (dateStr.length() == 8) {
            QString formattedDate = QString("%1-%2-%3")
                .arg(dateStr.mid(0, 4))
                .arg(dateStr.mid(4, 2))
                .arg(dateStr.mid(6, 2));
            displayName.replace(match.captured(1), formattedDate);
        }
        
        // Format time as HH:MM:SS
        if (timeStr.length() == 6) {
            QString formattedTime = QString("%1:%2:%3")
                .arg(timeStr.mid(0, 2))
                .arg(timeStr.mid(2, 2))
                .arg(timeStr.mid(4, 2));
            displayName.replace(match.captured(2), formattedTime);
        }
    }
    
    return displayName;
}

bool ExportedVideo::deleteVideo()
{
    QFileInfo fileInfo(m_filePath);
    if (!fileInfo.exists()) {
        return false;
    }
    
    QFile file(m_filePath);
    if (file.remove()) {
        emit videoDeleted();
        return true;
    }
    
    return false;
}

void ExportedVideo::openInExplorer() const
{
    QFileInfo fileInfo(m_filePath);
    if (!fileInfo.exists()) {
        return;
    }
    
#ifdef Q_OS_WIN
    // Windows: Open explorer with file selected
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(m_filePath)});
#elif defined(Q_OS_LINUX)
    // Linux: Open file manager with file selected
    QProcess::startDetached("xdg-open", {fileInfo.absolutePath()});
#elif defined(Q_OS_MACOS)
    // macOS: Open Finder with file selected
    QProcess::startDetached("open", {"-R", m_filePath});
#else
    // Fallback: Open directory
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
#endif
}

void ExportedVideo::playWithDefaultPlayer() const
{
    QDesktopServices::openUrl(m_fileUrl);
}

// ExportedVideoManager implementation
ExportedVideoManager::ExportedVideoManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_isScanning(false)
    , m_totalSize(0)
    , m_watcher(new QFileSystemWatcher(this))
{
    // Set default export directory
    QString videosPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    m_exportDirectory = QDir(videosPath).absoluteFilePath("Steam Recording Exports");
    
    // Ensure export directory exists
    QDir().mkpath(m_exportDirectory);
    
    // Setup file system watcher
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, 
            this, &ExportedVideoManager::onDirectoryChanged);
    
    if (QDir(m_exportDirectory).exists()) {
        m_watcher->addPath(m_exportDirectory);
    }
    
    // Initial scan
    refreshVideos();
}

ExportedVideoManager::~ExportedVideoManager()
{
    clearVideos();
}

int ExportedVideoManager::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_videos.size();
}

QVariant ExportedVideoManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_videos.size()) {
        return QVariant();
    }
    
    ExportedVideo *video = m_videos.at(index.row());
    
    switch (role) {
    case VideoRole:
        return QVariant::fromValue(video);
    case FilePathRole:
        return video->filePath();
    case FileNameRole:
        return video->fileName();
    case DisplayNameRole:
        return video->displayName();
    case FileSizeRole:
        return video->fileSize();
    case FileSizeStringRole:
        return video->fileSizeString();
    case CreationTimeRole:
        return video->creationTime();
    case CreationTimeStringRole:
        return video->creationTimeString();
    case FileUrlRole:
        return video->fileUrl();
    case ThumbnailPathRole:
        return video->thumbnailPath();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ExportedVideoManager::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[VideoRole] = "video";
    roles[FilePathRole] = "filePath";
    roles[FileNameRole] = "fileName";
    roles[DisplayNameRole] = "displayName";
    roles[FileSizeRole] = "fileSize";
    roles[FileSizeStringRole] = "fileSizeString";
    roles[CreationTimeRole] = "creationTime";
    roles[CreationTimeStringRole] = "creationTimeString";
    roles[FileUrlRole] = "fileUrl";
    roles[ThumbnailPathRole] = "thumbnailPath";
    return roles;
}

void ExportedVideoManager::setExportDirectory(const QString &directory)
{
    if (m_exportDirectory == directory) {
        return;
    }
    
    // Remove old directory from watcher
    if (!m_exportDirectory.isEmpty() && QDir(m_exportDirectory).exists()) {
        m_watcher->removePath(m_exportDirectory);
    }
    
    m_exportDirectory = directory;
    
    // Add new directory to watcher
    if (QDir(m_exportDirectory).exists()) {
        m_watcher->addPath(m_exportDirectory);
    }
    
    emit exportDirectoryChanged();
    refreshVideos();
}

void ExportedVideoManager::refreshVideos()
{
    if (m_isScanning) {
        return;
    }
    
    setIsScanning(true);
    setLastError("");
    
    // Clear current videos
    beginResetModel();
    clearVideos();
    endResetModel();
    
    // Scan directory
    scanDirectory();
    
    setIsScanning(false);
    emit scanCompleted();
}

void ExportedVideoManager::deleteVideo(int index)
{
    if (index < 0 || index >= m_videos.size()) {
        return;
    }
    
    ExportedVideo *video = m_videos.at(index);
    if (video->deleteVideo()) {
        removeVideo(index);
    }
}

void ExportedVideoManager::deleteAllVideos()
{
    if (m_videos.isEmpty()) {
        return;
    }
    
    beginResetModel();
    
    for (auto *video : m_videos) {
        video->deleteVideo();
    }
    
    clearVideos();
    endResetModel();
    updateTotalSize();
}

ExportedVideo* ExportedVideoManager::getVideo(int index) const
{
    if (index < 0 || index >= m_videos.size()) {
        return nullptr;
    }
    return m_videos.at(index);
}

void ExportedVideoManager::openExportDirectory() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_exportDirectory));
}

void ExportedVideoManager::sortByName()
{
    if (m_videos.isEmpty()) {
        return;
    }
    
    beginResetModel();
    
    QCollator collator;
    collator.setNumericMode(true);
    
    std::sort(m_videos.begin(), m_videos.end(), 
              [&collator](const ExportedVideo *a, const ExportedVideo *b) {
                  return collator.compare(a->displayName(), b->displayName()) < 0;
              });
    
    endResetModel();
}

void ExportedVideoManager::sortByDate()
{
    if (m_videos.isEmpty()) {
        return;
    }
    
    beginResetModel();
    
    std::sort(m_videos.begin(), m_videos.end(), 
              [](const ExportedVideo *a, const ExportedVideo *b) {
                  return a->creationTime() > b->creationTime(); // Newest first
              });
    
    endResetModel();
}

void ExportedVideoManager::sortBySize()
{
    if (m_videos.isEmpty()) {
        return;
    }
    
    beginResetModel();
    
    std::sort(m_videos.begin(), m_videos.end(), 
              [](const ExportedVideo *a, const ExportedVideo *b) {
                  return a->fileSize() > b->fileSize(); // Largest first
              });
    
    endResetModel();
}

void ExportedVideoManager::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path)
    // Auto-refresh when directory changes
    QTimer::singleShot(500, this, &ExportedVideoManager::refreshVideos);
}

void ExportedVideoManager::onVideoDeleted()
{
    ExportedVideo *video = qobject_cast<ExportedVideo*>(sender());
    if (!video) {
        return;
    }
    
    int index = m_videos.indexOf(video);
    if (index != -1) {
        removeVideo(index);
    }
}

void ExportedVideoManager::scanDirectory()
{
    QDir exportDir(m_exportDirectory);
    if (!exportDir.exists()) {
        setLastError(tr("Export directory does not exist: %1").arg(m_exportDirectory));
        emit scanFailed(m_lastError);
        return;
    }
    
    // Get video files from directory
    QStringList nameFilters;
    nameFilters << "*.mp4" << "*.avi" << "*.mkv" << "*.mov" << "*.webm" << "*.flv" << "*.wmv";
    
    QFileInfoList fileInfos = exportDir.entryInfoList(nameFilters, 
                                                      QDir::Files | QDir::Readable, 
                                                      QDir::Time | QDir::Reversed);
    
    beginInsertRows(QModelIndex(), 0, fileInfos.size() - 1);
    
    for (const QFileInfo &fileInfo : fileInfos) {
        if (isVideoFile(fileInfo.absoluteFilePath())) {
            addVideo(fileInfo.absoluteFilePath());
        }
    }
    
    endInsertRows();
    updateTotalSize();
}

void ExportedVideoManager::addVideo(const QString &filePath)
{
    ExportedVideo *video = new ExportedVideo(filePath, this);
    connect(video, &ExportedVideo::videoDeleted, this, &ExportedVideoManager::onVideoDeleted);
    
    m_videos.append(video);
    emit videoAdded(video);
}

void ExportedVideoManager::removeVideo(int index)
{
    if (index < 0 || index >= m_videos.size()) {
        return;
    }
    
    beginRemoveRows(QModelIndex(), index, index);
    
    ExportedVideo *video = m_videos.takeAt(index);
    video->deleteLater();
    
    endRemoveRows();
    
    emit videoRemoved(index);
    updateTotalSize();
}

void ExportedVideoManager::clearVideos()
{
    for (auto *video : m_videos) {
        video->deleteLater();
    }
    m_videos.clear();
    m_totalSize = 0;
    m_totalSizeString = QLocale().formattedDataSize(m_totalSize);
    emit totalSizeChanged();
    emit totalSizeStringChanged();
    emit videoCountChanged();
}

void ExportedVideoManager::updateTotalSize()
{
    m_totalSize = 0;
    for (const auto *video : m_videos) {
        m_totalSize += video->fileSize();
    }
    
    m_totalSizeString = QLocale().formattedDataSize(m_totalSize);
    emit totalSizeChanged();
    emit totalSizeStringChanged();
    emit videoCountChanged();
}

void ExportedVideoManager::setIsScanning(bool scanning)
{
    if (m_isScanning == scanning) {
        return;
    }
    
    m_isScanning = scanning;
    emit isScanningChanged();
}

void ExportedVideoManager::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    
    m_lastError = error;
    emit lastErrorChanged();
}



bool ExportedVideoManager::isVideoFile(const QString &filePath) const
{
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    
    return mimeType.name().startsWith("video/");
}