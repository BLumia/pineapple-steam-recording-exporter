#include "recordingclip.h"
#include "gameinfo.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QHash>
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
    QString mpdPath = QDir(segmentPath).absoluteFilePath("session.mpd");
    return ensureStaticMpd(mpdPath);
}

QUrl RecordingClip::getSegmentMpdUrl(int segmentIndex) const
{
    QString mpdPath = getSegmentMpdPath(segmentIndex);
    if (mpdPath.isEmpty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(mpdPath);
}

/*
 * Steam writes the session.mpd of clips cut from a background recording
 * (bg_*) with a Period that starts at the recording timeline offset, beyond
 * the end of the (much shorter) presentation, and some manifests also lack
 * a usable presentation duration. In both cases FFmpeg's DASH demuxer -
 * which is used for the in-app preview (via Qt Multimedia) as well as for
 * exporting - computes an empty playback window and only ever reads the
 * first media segment (~2-3s of content), no matter how long the recording
 * actually is.
 *
 * When such a manifest is detected, rewrite it into an equivalent one that
 * starts at zero with an explicit presentation duration, and hand out the
 * rewritten copy instead. The copy is placed next to the original so that
 * the relative segment URLs inside the manifest keep resolving.
 */
QString RecordingClip::ensureStaticMpd(const QString &mpdPath) const
{
    QFile file(mpdPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return mpdPath;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const int tagStart = content.indexOf(QLatin1String("<MPD"));
    const int tagEnd = tagStart >= 0 ? content.indexOf(QLatin1Char('>'), tagStart) : -1;
    if (tagEnd < 0) {
        return mpdPath;
    }
    const QString mpdTag = content.mid(tagStart, tagEnd - tagStart);

    const int periodTagStart = content.indexOf(QLatin1String("<Period"));
    const int periodTagEnd = periodTagStart >= 0 ? content.indexOf(QLatin1Char('>'), periodTagStart) : -1;
    const QString periodTag = periodTagEnd > periodTagStart
        ? content.mid(periodTagStart, periodTagEnd - periodTagStart) : QString();

    // Some (beta era) manifests written by Steam carry garbage after the
    // closing </MPD> tag. FFmpeg's XML parser rejects the whole document
    // ("Extra content at the end of the document"), so preview and export
    // fail to open such a manifest at all.
    const int docEnd = content.lastIndexOf(QLatin1String("</MPD>"));
    const bool hasTrailingGarbage = docEnd >= 0
        && content.mid(docEnd + 6).contains(QRegularExpression(QStringLiteral("\\S")));

    // Only rewrite when the manifest is not usable by FFmpeg as-is
    static const QRegularExpression typeRe(
        QStringLiteral("[\\s\"]type=\"([^\"]*)\""));
    const auto typeMatch = typeRe.match(mpdTag);
    const bool isStatic = !typeMatch.hasMatch() || typeMatch.captured(1) == QLatin1String("static");
    const double manifestDuration = parsePtDurationSeconds(
        xmlAttributeValue(mpdTag, QStringLiteral("mediaPresentationDuration")));
    const double periodStart = parsePtDurationSeconds(
        xmlAttributeValue(periodTag, QStringLiteral("start")));
    if (isStatic && manifestDuration > 0.0 && periodStart <= 0.0 && !hasTrailingGarbage) {
        return mpdPath;
    }

    const double durationSeconds = usableMpdDurationSeconds(content, QFileInfo(mpdPath).absolutePath());
    if (durationSeconds <= 0.0) {
        return mpdPath;
    }

    QString fixedTag = mpdTag;
    if (typeMatch.hasMatch()) {
        fixedTag.replace(typeRe, QStringLiteral(" type=\"static\""));
    }
    if (!fixedTag.contains(QStringLiteral(" type=\"static\""))) {
        fixedTag += QStringLiteral(" type=\"static\"");
    }
    static const QRegularExpression durationStripRe(
        QStringLiteral("[\\s\"]mediaPresentationDuration=\"P[^\"]*\""));
    fixedTag.replace(durationStripRe, QString());
    fixedTag += QStringLiteral(" mediaPresentationDuration=\"PT%1S\"")
                   .arg(durationSeconds, 0, 'f', 3);

    // Clamp the Period to the beginning of the presentation; FFmpeg computes
    // the playable window as (presentation duration - period start) and the
    // segment timestamps themselves are not affected by this attribute.
    QString fixedContent = content.left(tagStart) + fixedTag + content.mid(tagEnd);
    const int periodTagOffset = fixedContent.indexOf(QLatin1String("<Period"));
    if (periodTagOffset >= 0) {
        const int periodTagEndOffset = fixedContent.indexOf(QLatin1Char('>'), periodTagOffset);
        if (periodTagEndOffset > periodTagOffset) {
            QString fixedPeriodTag = fixedContent.mid(periodTagOffset, periodTagEndOffset - periodTagOffset);
            static const QRegularExpression periodStartRe(
                QStringLiteral("[\\s\"]start=\"P[^\"]*\""));
            if (periodStartRe.match(fixedPeriodTag).hasMatch()) {
                fixedPeriodTag.replace(periodStartRe, QStringLiteral(" start=\"PT0S\""));
            } else {
                fixedPeriodTag += QStringLiteral(" start=\"PT0S\"");
            }
            fixedContent = fixedContent.left(periodTagOffset) + fixedPeriodTag
                           + fixedContent.mid(periodTagEndOffset);
        }
    }

    // Give FFmpeg's seek arithmetic the real (background recording shifted)
    // timeline. FFmpeg hands seek targets to the demuxer as absolute stream
    // timestamps, and dashdec maps those to segments through the entries of
    // a SegmentTimeline (taking their t start time into account). Without
    // one, the $Number$-based arithmetic treats the absolute target as
    // zero-based and selects segments beyond the end of the recording,
    // which breaks seeking and any resume-from-position entirely.
    if (periodStart > 0.0) {
        const int chunkCount = countMediaChunks(QFileInfo(mpdPath).absolutePath());
        if (chunkCount > 0) {
            static const QRegularExpression segTplRe(
                QStringLiteral("<SegmentTemplate[^>]*>"));
            QString rebuilt;
            qsizetype lastPos = 0;
            auto templateIt = segTplRe.globalMatch(fixedContent);
            while (templateIt.hasNext()) {
                const auto match = templateIt.next();
                rebuilt += fixedContent.mid(lastPos, match.capturedStart(0) - lastPos);
                lastPos = match.capturedEnd(0);

                QString tag = match.captured(0);
                if (!tag.endsWith(QLatin1String("/>"))) {
                    // already carries children (e.g. its own timeline)
                    rebuilt += tag;
                    continue;
                }
                bool okScale = false;
                bool okDuration = false;
                const qint64 timescale = xmlAttributeValue(tag, QStringLiteral("timescale")).toLongLong(&okScale);
                const qint64 segmentDuration = xmlAttributeValue(tag, QStringLiteral("duration")).toLongLong(&okDuration);
                if (!okScale || timescale <= 0 || !okDuration || segmentDuration <= 0) {
                    rebuilt += tag;
                    continue;
                }

                const qint64 timelineStart = qRound64(periodStart * double(timescale));
                QString timeline = QStringLiteral("<SegmentTimeline><S t=\"%1\" d=\"%2\"")
                                       .arg(timelineStart).arg(segmentDuration);
                if (chunkCount > 1) {
                    timeline += QStringLiteral(" r=\"%1\"").arg(chunkCount - 1);
                }
                timeline += QStringLiteral("/></SegmentTimeline>");

                tag.chop(2); // remove the self-closing "/>"
                tag += QLatin1Char('>') + timeline + QStringLiteral("</SegmentTemplate>");
                rebuilt += tag;
            }
            rebuilt += fixedContent.mid(lastPos);
            fixedContent = rebuilt;
        }
    }

    // Drop any garbage after the closing </MPD> tag (see hasTrailingGarbage)
    const int fixedDocEnd = fixedContent.lastIndexOf(QLatin1String("</MPD>"));
    if (fixedDocEnd >= 0) {
        fixedContent = fixedContent.left(fixedDocEnd + 6) + QLatin1Char('\n');
    }

    const QFileInfo info(mpdPath);
    const QString fixedPath = info.absolutePath() + QLatin1Char('/')
                              + info.completeBaseName() + QLatin1String(".psre.mpd");

    // Reuse the copy from a previous run when it is still up to date
    // (identical content), so the format can also be re-generated after
    // application updates
    QFile existing(fixedPath);
    if (existing.open(QIODevice::ReadOnly)) {
        if (QString::fromUtf8(existing.readAll()) == fixedContent) {
            return fixedPath;
        }
        existing.close();
    }

    QFile out(fixedPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // e.g. read-only location; the original path is better than nothing
        return mpdPath;
    }
    out.write(fixedContent.toUtf8());
    out.close();

    qInfo() << "RecordingClip: rewrote manifest not playable by FFmpeg:"
            << mpdPath << "->" << fixedPath
            << QString("(duration PT%1S)").arg(durationSeconds, 0, 'f', 3);
    return fixedPath;
}

double RecordingClip::usableMpdDurationSeconds(const QString &mpdContent, const QString &segmentDir) const
{
    // Prefer the duration stated by the manifest itself
    const int tagStart = mpdContent.indexOf(QLatin1String("<MPD"));
    const int tagEnd = tagStart >= 0 ? mpdContent.indexOf(QLatin1Char('>'), tagStart) : -1;
    if (tagEnd > tagStart) {
        const double manifestDuration = parsePtDurationSeconds(xmlAttributeValue(
            mpdContent.mid(tagStart, tagEnd - tagStart),
            QStringLiteral("mediaPresentationDuration")));
        if (manifestDuration > 0.0) {
            return manifestDuration;
        }
    }
    return estimateMpdDurationSeconds(mpdContent, segmentDir);
}

double RecordingClip::estimateMpdDurationSeconds(const QString &mpdContent, const QString &segmentDir) const
{
    // Preferred source of truth: SegmentTemplate@duration multiplied by the
    // number of media chunks that actually exist on disk. The last chunk is
    // usually shorter than the template duration, so this may overestimate
    // by up to one segment; FFmpeg simply stops at the real end of stream.
    qint64 timescale = 0;
    const int tplStart = mpdContent.indexOf(QLatin1String("<SegmentTemplate"));
    if (tplStart >= 0) {
        const int tplEnd = mpdContent.indexOf(QLatin1Char('>'), tplStart);
        if (tplEnd > tplStart) {
            const QString tpl = mpdContent.mid(tplStart, tplEnd - tplStart);
            bool okScale = false;
            bool okDuration = false;
            timescale = xmlAttributeValue(tpl, QStringLiteral("timescale")).toLongLong(&okScale);
            const qint64 templateDuration = xmlAttributeValue(tpl, QStringLiteral("duration")).toLongLong(&okDuration);
            if (okScale && okDuration && timescale > 0 && templateDuration > 0) {
                const int chunks = countMediaChunks(segmentDir);
                if (chunks > 0) {
                    return double(chunks) * double(templateDuration) / double(timescale);
                }
            }
        }
    }

    // Fallback: SegmentTimeline entries (<S d="..." r="..."/>). Take the
    // largest total among the (per representation) timelines found.
    static const QRegularExpression timelineRe(
        QStringLiteral("<SegmentTimeline[^>]*>(.*?)</SegmentTimeline>"));
    static const QRegularExpression entryRe(
        QStringLiteral("<S[^>]*?[\\s\"]d=\"(\\d+)\"([^>]*)>"));
    static const QRegularExpression repeatRe(
        QStringLiteral("[\\s\"]r=\"(\\d+)\""));
    qint64 bestTotal = 0;
    auto timelineIt = timelineRe.globalMatch(mpdContent);
    while (timelineIt.hasNext()) {
        const auto timelineMatch = timelineIt.next();
        qint64 total = 0;
        auto entryIt = entryRe.globalMatch(timelineMatch.captured(1));
        while (entryIt.hasNext()) {
            const auto entryMatch = entryIt.next();
            qint64 repeats = 1;
            const auto repeatMatch = repeatRe.match(entryMatch.captured(2));
            if (repeatMatch.hasMatch()) {
                repeats = qMax<qint64>(1, repeatMatch.captured(1).toLongLong() + 1);
            }
            total += entryMatch.captured(1).toLongLong() * repeats;
        }
        bestTotal = qMax(bestTotal, total);
    }
    if (bestTotal > 0) {
        // DASH defaults the timescale to 1 when absent
        return double(bestTotal) / double(timescale > 0 ? timescale : 1);
    }

    return 0.0;
}

int RecordingClip::countMediaChunks(const QString &segmentDir) const
{
    // Media chunks follow "chunk-<something>-<number>.m4s" (from the
    // $RepresentationID$/$Number$ media template Steam writes). Count them
    // per representation and return the largest count; audio and video
    // representations may differ by one chunk at the tail.
    const QStringList files = QDir(segmentDir).entryList(QStringList() << "chunk-*", QDir::Files);
    static const QRegularExpression numberSuffixRe(
        QStringLiteral("-\\d+\\.[^.]+$"));

    QHash<QString, int> perRepresentation;
    for (const QString &fileName : files) {
        QString representation = fileName;
        representation.remove(numberSuffixRe);
        if (representation == fileName) {
            continue; // does not follow the expected naming scheme
        }
        perRepresentation[representation]++;
    }

    int maxChunks = 0;
    for (auto it = perRepresentation.cbegin(); it != perRepresentation.cend(); ++it) {
        maxChunks = qMax(maxChunks, it.value());
    }
    return maxChunks;
}

QString RecordingClip::xmlAttributeValue(const QString &tagText, const QString &name)
{
    const QRegularExpression attrRe(
        QStringLiteral("[\\s\"]%1=\"([^\"]*)\"").arg(QRegularExpression::escape(name)));
    const auto match = attrRe.match(tagText);
    return match.hasMatch() ? match.captured(1) : QString();
}

// Parses ISO 8601 durations of the form PT#H#M#.#S, as written by Steam
// (e.g. "PT3M27.231S"). Returns 0.0 for anything unparseable.
double RecordingClip::parsePtDurationSeconds(const QString &ptValue)
{
    if (!ptValue.startsWith(QLatin1Char('P'))) {
        return 0.0;
    }
    static const QRegularExpression re(
        QStringLiteral("^P(?:T(?:(\\d+(?:\\.\\d+)?)H)?(?:(\\d+(?:\\.\\d+)?)M)?(?:(\\d+(?:\\.\\d+)?)S)?)?$"));
    const auto match = re.match(ptValue);
    if (!match.hasMatch()) {
        return 0.0;
    }
    const double hours = match.captured(1).isEmpty() ? 0.0 : match.captured(1).toDouble();
    const double minutes = match.captured(2).isEmpty() ? 0.0 : match.captured(2).toDouble();
    const double seconds = match.captured(3).isEmpty() ? 0.0 : match.captured(3).toDouble();
    return hours * 3600.0 + minutes * 60.0 + seconds;
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
            // Find all segment directories. Recording directories should start with:
            // - "fg_" for foreground/manual recording, or
            // - "bg_" for background recording (user need to manually save them to Steam to make it shows up)
            QStringList segmentDirs = videoDir.entryList(QStringList{"fg_*", "bg_*"}, QDir::Dirs | QDir::NoDotAndDotDot);
            
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
    // Prefer the duration stated by the recording's own manifest; it is
    // exact and also covers the segments' real timeline offsets
    double newDuration = 0;
    bool haveMpdDuration = false;
    for (const QString &segmentPath : m_segments) {
        QFile mpdFile(QDir(segmentPath).absoluteFilePath("session.mpd"));
        if (!mpdFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString content = QString::fromUtf8(mpdFile.readAll());
        const double duration = usableMpdDurationSeconds(content, segmentPath);
        if (duration > 0.0) {
            newDuration += duration;
            haveMpdDuration = true;
        }
    }

    // Fallback: rough estimate from the number of media files, assuming
    // ~2 seconds per file (highly variable)
    if (!haveMpdDuration && !m_segments.isEmpty()) {
        for (const QString &segmentPath : m_segments) {
            QDir segmentDir(segmentPath);
            QStringList mediaFiles = segmentDir.entryList(QStringList() << "*.m4s" << "*.mp4" << "*.webm", QDir::Files);
            newDuration += mediaFiles.size() * 2;
        }

        // Ensure we have at least some duration if we have segments
        if (newDuration == 0 && !m_segments.isEmpty()) {
            newDuration = 60; // Default to 60 seconds if we can't estimate
        }
    }

    const int newDurationSeconds = int(newDuration);
    if (m_duration != newDurationSeconds) {
        m_duration = newDurationSeconds;
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
