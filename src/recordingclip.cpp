#include "recordingclip.h"
#include "gameinfo.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QLocale>
#include <QUrl>
#include <QDirIterator>

RecordingClip::RecordingClip(QObject *parent)
    : QObject(parent)
    , m_totalSize(0)
    , m_duration(0)
    , m_formattedDateCached(false)
    , m_formattedSizeCached(false)
    , m_formattedDurationCached(false)
{
}

RecordingClip::RecordingClip(const QString &clipPath, QObject *parent)
    : QObject(parent)
    , m_totalSize(0)
    , m_duration(0)
    , m_formattedDateCached(false)
    , m_formattedSizeCached(false)
    , m_formattedDurationCached(false)
{
    loadFromPath(clipPath);
}

void RecordingClip::setAppId(const QString &appId)
{
    if (m_appId != appId) {
        m_appId = appId;
        emit appIdChanged();
        emit isValidChanged();
    }
}

void RecordingClip::setClipPath(const QString &clipPath)
{
    if (m_clipPath != clipPath) {
        m_clipPath = clipPath;
        emit clipPathChanged();
        emit isValidChanged();
        
        // Clear cached values
        m_formattedDateCached = false;
        m_formattedSizeCached = false;
        m_formattedDurationCached = false;
        
        parseClipPath();
        findThumbnail();
        scanSegments();
        calculateTotalSize();
        estimateDuration();
    }
}

QString RecordingClip::formattedDate() const
{
    if (!m_formattedDateCached) {
        if (m_recordingDate.isValid()) {
            QLocale locale;
            m_cachedFormattedDate = locale.toString(m_recordingDate, QLocale::ShortFormat);
        } else {
            m_cachedFormattedDate = tr("Unknown Date");
        }
        m_formattedDateCached = true;
    }
    return m_cachedFormattedDate;
}

QString RecordingClip::formattedSize() const
{
    if (!m_formattedSizeCached) {
        m_cachedFormattedSize = QLocale().formattedDataSize(m_totalSize);
        m_formattedSizeCached = true;
    }
    return m_cachedFormattedSize;
}

QString RecordingClip::formattedDuration() const
{
    if (!m_formattedDurationCached) {
        m_cachedFormattedDuration = formatDuration(m_duration);
        m_formattedDurationCached = true;
    }
    return m_cachedFormattedDuration;
}

bool RecordingClip::loadFromPath(const QString &clipPath)
{
    if (clipPath.isEmpty()) {
        qWarning() << "RecordingClip::loadFromPath: Empty clip path";
        return false;
    }

    QFileInfo clipInfo(clipPath);
    if (!clipInfo.exists() || !clipInfo.isDir()) {
        qWarning() << "RecordingClip::loadFromPath: Clip path does not exist or is not a directory:" << clipPath;
        return false;
    }

    setClipPath(clipPath);
    return isValid();
}

QString RecordingClip::getSegmentMpdPath(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_segments.size()) {
        return QString();
    }

    QString segmentPath = m_segments.at(segmentIndex);
    return QDir(segmentPath).absoluteFilePath("session.mpd");
}

QUrl RecordingClip::getSegmentMpdUrl(int segmentIndex) const
{
    QString mpdPath = getSegmentMpdPath(segmentIndex);
    if (mpdPath.isEmpty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(mpdPath);
}

QUrl RecordingClip::getThumbnailUrl() const
{
    if (m_thumbnailPath.isEmpty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(m_thumbnailPath);
}

void RecordingClip::refreshSegments()
{
    scanSegments();
    calculateTotalSize();
    estimateDuration();
}

void RecordingClip::refreshGameInfo(const QString &steamPath)
{
    if (m_appId.isEmpty() || steamPath.isEmpty()) {
        return;
    }

    GameInfo *gameInfo = GameInfo::fromAppId(m_appId, steamPath, this);
    if (gameInfo) {
        QString newGameName = gameInfo->name();
        if (m_gameName != newGameName) {
            m_gameName = newGameName;
            emit gameNameChanged();
        }
        gameInfo->deleteLater();
    }
}

RecordingClip* RecordingClip::fromClipPath(const QString &clipPath, QObject *parent)
{
    RecordingClip *clip = new RecordingClip(parent);
    if (clip->loadFromPath(clipPath)) {
        return clip;
    } else {
        delete clip;
        return nullptr;
    }
}

QList<RecordingClip*> RecordingClip::findAllClips(const QString &gameRecordingsPath, QObject *parent)
{
    QList<RecordingClip*> clips;

    QDir recordingsDir(gameRecordingsPath);
    if (!recordingsDir.exists()) {
        qWarning() << "Game recordings directory does not exist:" << gameRecordingsPath;
        return clips;
    }

    QDir clipsDir = recordingsDir;
    if (!clipsDir.cd("clips")) {
        qWarning() << "Clips directory does not exist in:" << gameRecordingsPath;
        return clips;
    }

    // Find all clip directories (should start with "clip_")
    QStringList clipDirs = clipsDir.entryList(QStringList() << "clip_*", QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &clipDirName : clipDirs) {
        QString clipPath = clipsDir.absoluteFilePath(clipDirName);
        RecordingClip *clip = RecordingClip::fromClipPath(clipPath, parent);
        
        if (clip) {
            clips.append(clip);
            qDebug() << "Found clip:" << clipDirName << "AppID:" << clip->appId();
        } else {
            qWarning() << "Failed to load clip from:" << clipPath;
        }
    }

    // Sort clips by recording date (newest first)
    std::sort(clips.begin(), clips.end(), [](const RecordingClip *a, const RecordingClip *b) {
        return a->recordingDate() > b->recordingDate();
    });

    qDebug() << "Found" << clips.size() << "recording clips";
    return clips;
}

void RecordingClip::parseClipPath()
{
    if (m_clipPath.isEmpty()) {
        return;
    }

    QFileInfo clipInfo(m_clipPath);
    QString clipDirName = clipInfo.fileName();

    // Expected format: clip_<appid>_<date>_<time>
    // Example: clip_3527290_20251001_073810
    QRegularExpression regex(R"(clip_(\d+)_(\d{8})_(\d{6}))");
    QRegularExpressionMatch match = regex.match(clipDirName);

    if (match.hasMatch()) {
        QString appId = match.captured(1);
        QString dateStr = match.captured(2); // YYYYMMDD
        QString timeStr = match.captured(3); // HHMMSS

        setAppId(appId);

        QDateTime recordingDateTime = parseRecordingDateTime(dateStr, timeStr);
        if (m_recordingDate != recordingDateTime) {
            m_recordingDate = recordingDateTime;
            emit recordingDateChanged();
            emit formattedDateChanged();
            m_formattedDateCached = false;
        }

        qDebug() << "Parsed clip:" << clipDirName << "AppID:" << appId << "Date:" << recordingDateTime;
    } else {
        qWarning() << "Failed to parse clip directory name:" << clipDirName;
    }
}

void RecordingClip::findThumbnail()
{
    if (m_clipPath.isEmpty()) {
        return;
    }

    QString thumbnailPath = QDir(m_clipPath).absoluteFilePath("thumbnail.jpg");
    
    QString newThumbnailPath;
    if (QFileInfo::exists(thumbnailPath)) {
        newThumbnailPath = thumbnailPath;
    }

    if (m_thumbnailPath != newThumbnailPath) {
        m_thumbnailPath = newThumbnailPath;
        emit thumbnailPathChanged();
    }
}

void RecordingClip::scanSegments()
{
    QStringList newSegments;

    if (!m_clipPath.isEmpty()) {
        QDir clipDir(m_clipPath);
        QDir videoDir = clipDir;
        
        if (videoDir.cd("video")) {
            // Find all segment directories (should start with "fg_")
            QStringList segmentDirs = videoDir.entryList(QStringList() << "fg_*", QDir::Dirs | QDir::NoDotAndDotDot);
            
            for (const QString &segmentDirName : segmentDirs) {
                QString segmentPath = videoDir.absoluteFilePath(segmentDirName);
                
                // Check if session.mpd exists in this segment
                QString mpdPath = QDir(segmentPath).absoluteFilePath("session.mpd");
                if (QFileInfo::exists(mpdPath)) {
                    newSegments.append(segmentPath);
                    qDebug() << "Found segment:" << segmentDirName << "in" << m_clipPath;
                }
            }
            
            // Sort segments by name to ensure consistent ordering
            newSegments.sort();
        }
    }

    if (m_segments != newSegments) {
        m_segments = newSegments;
        emit segmentsChanged();
        emit segmentCountChanged();
    }
}

void RecordingClip::calculateTotalSize()
{
    qint64 newTotalSize = 0;

    for (const QString &segmentPath : m_segments) {
        QDirIterator it(segmentPath, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFileInfo fileInfo = it.fileInfo();
            if (fileInfo.isFile()) {
                newTotalSize += fileInfo.size();
            }
        }
    }

    if (m_totalSize != newTotalSize) {
        m_totalSize = newTotalSize;
        emit totalSizeChanged();
        emit formattedSizeChanged();
        m_formattedSizeCached = false;
    }
}

void RecordingClip::estimateDuration()
{
    // This is a basic estimation - in a real implementation, you might want to
    // parse the MPD files or use FFmpeg to get accurate duration
    int newDuration = 0;

    if (!m_segments.isEmpty()) {
        // Estimate based on file count and typical segment length
        // This is a rough approximation - actual implementation should parse media metadata
        for (const QString &segmentPath : m_segments) {
            QDir segmentDir(segmentPath);
            QStringList mediaFiles = segmentDir.entryList(QStringList() << "*.m4s" << "*.mp4" << "*.webm", QDir::Files);
            
            // Rough estimate: each media file represents ~2 seconds (this is highly variable)
            newDuration += mediaFiles.size() * 2;
        }
        
        // Ensure we have at least some duration if we have segments
        if (newDuration == 0 && !m_segments.isEmpty()) {
            newDuration = 60; // Default to 60 seconds if we can't estimate
        }
    }

    if (m_duration != newDuration) {
        m_duration = newDuration;
        emit durationChanged();
        emit formattedDurationChanged();
        m_formattedDurationCached = false;
    }
}

QDateTime RecordingClip::parseRecordingDateTime(const QString &dateStr, const QString &timeStr) const
{
    if (dateStr.length() != 8 || timeStr.length() != 6) {
        return QDateTime();
    }

    // Parse date: YYYYMMDD
    int year = dateStr.mid(0, 4).toInt();
    int month = dateStr.mid(4, 2).toInt();
    int day = dateStr.mid(6, 2).toInt();

    // Parse time: HHMMSS
    int hour = timeStr.mid(0, 2).toInt();
    int minute = timeStr.mid(2, 2).toInt();
    int second = timeStr.mid(4, 2).toInt();

    QDate date(year, month, day);
    QTime time(hour, minute, second);

    if (date.isValid() && time.isValid()) {
        return QDateTime(date, time);
    }

    return QDateTime();
}



QString RecordingClip::formatDuration(int seconds) const
{
    if (seconds <= 0) {
        return tr("00:00");
    }

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
}