#include "videoexporter.h"
#include "recordingclip.h"
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QLocale>

VideoExporter::VideoExporter(QObject *parent)
    : QObject(parent)
    , m_isExporting(false)
    , m_progress(0)
    , m_ffmpegProcess(new QProcess(this))
    , m_progressTimer(new QTimer(this))
    , m_startTime(0)
    , m_lastOutTimeMs(0)
    , m_totalDurationMs(0)
    , m_lastProgress(0)
    , m_isCancelling(false)
{
    // Set default export path
    m_defaultExportPath = getDefaultExportDirectory();
    
    // Set default ffmpeg path (assume it's in PATH)
    m_ffmpegPath = "ffmpeg";
    
    // Setup progress timer
    m_progressTimer->setInterval(1000); // Update every second
    connect(m_progressTimer, &QTimer::timeout, this, &VideoExporter::updateProgress);
    
    // Connect process signals
    connect(m_ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoExporter::onProcessFinished);
    connect(m_ffmpegProcess, &QProcess::errorOccurred,
            this, &VideoExporter::onProcessError);
    connect(m_ffmpegProcess, &QProcess::readyReadStandardOutput,
            this, &VideoExporter::onProcessReadyReadStandardOutput);
    connect(m_ffmpegProcess, &QProcess::readyReadStandardError,
            this, &VideoExporter::onProcessReadyReadStandardError);
}

VideoExporter::~VideoExporter()
{
    if (m_ffmpegProcess && m_ffmpegProcess->state() != QProcess::NotRunning) {
        m_ffmpegProcess->kill();
        m_ffmpegProcess->waitForFinished(3000);
    }
}

void VideoExporter::setDefaultExportPath(const QString &path)
{
    if (m_defaultExportPath != path) {
        m_defaultExportPath = path;
        emit defaultExportPathChanged();
    }
}

void VideoExporter::setFfmpegPath(const QString &path)
{
    if (m_ffmpegPath != path) {
        m_ffmpegPath = path;
        emit ffmpegPathChanged();
    }
}

bool VideoExporter::exportClipSegment(RecordingClip *clip, int segmentIndex, const QString &outputPath)
{
    if (!clip || !clip->isValid()) {
        setLastError(tr("Invalid recording clip"));
        return false;
    }

    if (segmentIndex < 0 || segmentIndex >= clip->segmentCount()) {
        setLastError(tr("Invalid segment index: %1").arg(segmentIndex));
        return false;
    }

    if (m_isExporting) {
        setLastError(tr("Export already in progress"));
        return false;
    }

    QString mpdPath = clip->getSegmentMpdPath(segmentIndex);
    if (mpdPath.isEmpty() || !validateInputFile(mpdPath)) {
        setLastError(tr("Cannot find or access segment MPD file"));
        return false;
    }

    QString finalOutputPath = outputPath;
    if (finalOutputPath.isEmpty()) {
        finalOutputPath = QDir(m_defaultExportPath).absoluteFilePath(
            generateOutputFilename(clip, segmentIndex)
        );
    }

    QString operationName = tr("Exporting segment %1 of %2")
        .arg(segmentIndex + 1)
        .arg(clip->gameName().isEmpty() ? clip->appId() : clip->gameName());

    return startExport(mpdPath, finalOutputPath, operationName);
}

bool VideoExporter::exportEntireClip(RecordingClip *clip, const QString &outputPath)
{
    if (!clip || !clip->isValid()) {
        setLastError(tr("Invalid recording clip"));
        return false;
    }

    if (m_isExporting) {
        setLastError(tr("Export already in progress"));
        return false;
    }

    if (clip->segmentCount() == 0) {
        setLastError(tr("No segments found in recording clip"));
        return false;
    }

    if (clip->segmentCount() == 1) {
        // Just export the single segment
        return exportClipSegment(clip, 0, outputPath);
    }

    // TODO: Implement multi-segment concatenation
    // For now, just export the first segment with a warning
    addToLog(tr("Warning: Multi-segment export not yet implemented. Exporting first segment only."));
    return exportClipSegment(clip, 0, outputPath);
}

void VideoExporter::cancelExport()
{
    if (!m_isExporting || !m_ffmpegProcess) {
        return;
    }

    addToLog(tr("Cancelling export..."));
    m_isCancelling = true;

    m_ffmpegProcess->kill();
    if (!m_ffmpegProcess->waitForFinished(3000)) {
        m_ffmpegProcess->terminate();
        m_ffmpegProcess->waitForFinished(1000);
    }

    flushPendingLog();
    resetState();
    emit exportCancelled();
}

QString VideoExporter::generateOutputFilename(RecordingClip *clip, int segmentIndex) const
{
    if (!clip) {
        return "recording.mp4";
    }

    QString baseName;
    if (!clip->gameName().isEmpty()) {
        baseName = clip->gameName();
    } else {
        baseName = QString("Game_%1").arg(clip->appId());
    }

    QString dateStr = clip->recordingDate().toString("yyyy-MM-dd_hh-mm-ss");
    
    if (segmentIndex >= 0) {
        baseName += QString("_segment_%1").arg(segmentIndex + 1);
    }
    
    if (!dateStr.isEmpty()) {
        baseName += "_" + dateStr;
    }

    return sanitizeFilename(baseName) + ".mp4";
}

QString VideoExporter::selectOutputPath(const QString &suggestedName) const
{
    // This would typically open a file dialog, but for now just return a path in the default directory
    QString filename = suggestedName.isEmpty() ? "recording.mp4" : suggestedName;
    return QDir(m_defaultExportPath).absoluteFilePath(filename);
}

bool VideoExporter::validateOutputPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(path);
    QDir parentDir = fileInfo.dir();
    
    if (!parentDir.exists()) {
        return parentDir.mkpath(".");
    }

    QFileInfo dirInfo(parentDir.absolutePath());
    return dirInfo.isWritable();
}

void VideoExporter::clearLog()
{
    m_pendingLogLines.clear();
    if (!m_exportLog.isEmpty()) {
        m_exportLog.clear();
        emit exportLogChanged();
    }
}

void VideoExporter::openExportLocation(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        // Open the file's directory and select the file
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }
}

QString VideoExporter::getDefaultExportDirectory()
{
    QString videosPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    QString exportPath = QDir(videosPath).absoluteFilePath("Steam Recording Exports");
    
    QDir().mkpath(exportPath);
    return exportPath;
}

QString VideoExporter::sanitizeFilename(const QString &filename)
{
    QString sanitized = filename;
    
    // Replace invalid filename characters
    QRegularExpression invalidChars("[<>:\"/\\\\|?*]");
    sanitized.replace(invalidChars, "_");
    
    // Remove control characters
    sanitized.remove(QRegularExpression("[\x00-\x1F\x7F]"));
    
    // Trim whitespace
    sanitized = sanitized.trimmed();
    
    // Ensure it's not empty
    if (sanitized.isEmpty()) {
        sanitized = "recording";
    }
    
    return sanitized;
}

void VideoExporter::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_progressTimer->stop();

    if (m_isCancelling) {
        // The process was killed on user request, don't report it as an error
        addToLog(tr("Export cancelled."));
        flushPendingLog();
        resetState();
        emit exportCancelled();
        return;
    }

    if (exitStatus == QProcess::CrashExit) {
        QString error = tr("FFmpeg process crashed");
        setLastError(error);
        addToLog(error);
        flushPendingLog();
        resetState();
        emit exportFailed(error);
        return;
    }

    if (exitCode == 0) {
        // Success
        addToLog(tr("Export completed successfully!"));
        
        QFileInfo outputFile(m_currentOutputPath);
        if (outputFile.exists()) {
            addToLog(tr("Output file: %1 (%2)")
                .arg(outputFile.fileName())
                .arg(QLocale().formattedDataSize(outputFile.size())));
        }
        flushPendingLog();

        QString outputPath = m_currentOutputPath;
        resetState();
        setProgress(100); // After resetState() so the final value stays visible
        emit exportCompleted(outputPath);
    } else {
        // Error
        QString error = tr("FFmpeg exited with code %1").arg(exitCode);
        setLastError(error);
        addToLog(error);
        flushPendingLog();
        resetState();
        emit exportFailed(error);
    }
}

void VideoExporter::onProcessError(QProcess::ProcessError error)
{
    m_progressTimer->stop();

    if (m_isCancelling) {
        // Error notifications triggered by killing the process on user
        // request are not real failures; onProcessFinished() reports the
        // cancellation.
        return;
    }

    QString errorString;
    switch (error) {
    case QProcess::FailedToStart:
        errorString = tr("Failed to start FFmpeg. Please check that FFmpeg is installed and accessible.");
        break;
    case QProcess::Crashed:
        errorString = tr("FFmpeg crashed during export");
        break;
    case QProcess::Timedout:
        errorString = tr("FFmpeg process timed out");
        break;
    case QProcess::WriteError:
        errorString = tr("Failed to write to FFmpeg process");
        break;
    case QProcess::ReadError:
        errorString = tr("Failed to read from FFmpeg process");
        break;
    case QProcess::UnknownError:
    default:
        errorString = tr("Unknown FFmpeg error occurred");
        break;
    }
    
    setLastError(errorString);
    addToLog(errorString);
    flushPendingLog();
    resetState();
    emit exportFailed(errorString);
}

void VideoExporter::onProcessReadyReadStandardOutput()
{
    if (!m_ffmpegProcess) {
        return;
    }

    handleProcessOutput(QString::fromUtf8(m_ffmpegProcess->readAllStandardOutput()));
}

void VideoExporter::onProcessReadyReadStandardError()
{
    if (!m_ffmpegProcess) {
        return;
    }

    // FFmpeg sends progress info to stderr, so we need to parse it
    handleProcessOutput(QString::fromUtf8(m_ffmpegProcess->readAllStandardError()));
}

void VideoExporter::updateProgress()
{
    // Called once per second while exporting. Progress values themselves are
    // pushed by handleProcessOutput(); use the tick to flush queued log lines
    // so the QML log view is refreshed at most once per second.
    flushPendingLog();
}

bool VideoExporter::startExport(const QString &inputMpdPath, const QString &outputPath, const QString &operationName)
{
    if (!validateFfmpegExecutable()) {
        setLastError(tr("FFmpeg executable not found or not accessible"));
        return false;
    }

    if (!validateInputFile(inputMpdPath)) {
        setLastError(tr("Input file not found or not accessible: %1").arg(inputMpdPath));
        return false;
    }

    QString finalOutputPath = ensureOutputDirectory(outputPath);
    if (finalOutputPath.isEmpty()) {
        setLastError(tr("Cannot create output directory"));
        return false;
    }

    // Reset state
    resetState();
    m_isCancelling = false;
    
    // Set up export
    m_currentInputPath = inputMpdPath;
    m_currentOutputPath = finalOutputPath;
    setCurrentOperation(operationName);
    setIsExporting(true);
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    
    addToLog(tr("Starting export: %1").arg(operationName));
    addToLog(tr("Input: %1").arg(inputMpdPath));
    addToLog(tr("Output: %1").arg(finalOutputPath));
    
    setupExportProcess(inputMpdPath, finalOutputPath);
    
    emit exportStarted(finalOutputPath);
    return true;
}

void VideoExporter::setupExportProcess(const QString &inputMpdPath, const QString &outputPath)
{
    QStringList arguments = buildFfmpegArguments(inputMpdPath, outputPath);
    
    addToLog(tr("FFmpeg command: %1 %2").arg(m_ffmpegPath, arguments.join(" ")));
    
    m_ffmpegProcess->start(m_ffmpegPath, arguments);
    m_progressTimer->start();
}

QStringList VideoExporter::buildFfmpegArguments(const QString &inputMpdPath, const QString &outputPath) const
{
    QStringList args;
    
    // Input file
    args << "-i" << inputMpdPath;
    
    // Fast copy mode - direct stream copy with optimizations
    args << "-c" << "copy";
    
    // Fix timestamps and seeking issues without re-encoding
    args << "-avoid_negative_ts" << "make_zero";   // Fix negative timestamps
    args << "-fflags" << "+genpts";                // Generate presentation timestamps for better seeking
    args << "-movflags" << "+faststart";           // Move moov atom to beginning for streaming/seeking
    
    // Overwrite output file without prompting
    args << "-y";
    
    // Progress reporting (machine readable key=value blocks on stderr).
    // '-nostats' suppresses the additional human readable status line.
    args << "-progress" << "pipe:2";
    args << "-nostats";
    
    // Output file
    args << outputPath;
    
    return args;
}

void VideoExporter::handleProcessOutput(const QString &output)
{
    m_progressBuffer += output;

    // FFmpeg terminates its human readable status line with '\r' (it only
    // uses '\n' when stderr is attached to a terminal), while the machine
    // readable '-progress' reports are '\n' separated. Only handle complete
    // lines and keep the remainder buffered until the rest arrives.
    int lastSeparator = -1;
    for (int i = m_progressBuffer.size() - 1; i >= 0; --i) {
        const QChar ch = m_progressBuffer.at(i);
        if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n')) {
            lastSeparator = i;
            break;
        }
    }
    if (lastSeparator < 0) {
        return;
    }

    const QString complete = m_progressBuffer.left(lastSeparator);
    m_progressBuffer = m_progressBuffer.mid(lastSeparator + 1);

    const QStringList lines = complete.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || parseProgressLine(line)) {
            continue; // Progress related lines never show up in the log
        }

        // FFmpeg prints one of these for every single DASH chunk it reads
        // (thousands of lines for a long recording); way too noisy to keep
        static const QRegularExpression openingRe(
            QStringLiteral("Opening '.*' for reading$"));
        if (openingRe.match(line).hasMatch()) {
            continue;
        }

        addToLog(line);
    }
}

bool VideoExporter::parseProgressLine(const QString &line)
{
    // Total duration from FFmpeg's input banner: "Duration: 00:01:23.45, ..."
    static const QRegularExpression durationRe(
        QStringLiteral("\\bDuration:\\s*(\\d+:\\d{1,2}:\\d{1,2}(?:\\.\\d+)?)"));
    const auto durationMatch = durationRe.match(line);
    if (durationMatch.hasMatch()) {
        const qint64 totalMs = parseTimeStringToMs(durationMatch.captured(1));
        if (totalMs > 0) {
            m_totalDurationMs = totalMs;
            if (m_lastOutTimeMs > 0) {
                // Recalculate now that the total duration is known
                updateProgressFromElapsedTime(m_lastOutTimeMs);
            }
        }
        return true;
    }

    // Machine readable report from '-progress pipe:2', one key=value per line
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq > 0) {
        const QString key = line.left(eq);
        const QString value = line.mid(eq + 1).trimmed();

        if (key == QLatin1String("out_time_us") || key == QLatin1String("out_time_ms")) {
            // Note: FFmpeg reports both of these keys in microseconds
            bool ok = false;
            const qint64 us = value.toLongLong(&ok);
            if (ok && us >= 0) {
                updateProgressFromElapsedTime(us / 1000);
            }
            return true;
        }
        if (key == QLatin1String("out_time")) {
            const qint64 ms = parseTimeStringToMs(value);
            if (ms >= 0) {
                updateProgressFromElapsedTime(ms);
            }
            return true;
        }

        // Other machine readable keys (frame, fps, bitrate, speed, progress,
        // ...) never contain spaces; consume them without logging. A human
        // readable status line however spans multiple key=value pairs.
        if (!line.startsWith(QLatin1String("frame=")) || !line.contains(QLatin1Char(' '))) {
            return true;
        }
    }

    // Human readable status line fallback (when '-progress' output is not
    // available): "frame= 123 fps= 30 ... time=00:00:04.12 bitrate= ..."
    static const QRegularExpression timeRe(
        QStringLiteral("(?:^|\\s)time=(\\d+:\\d{1,2}:\\d{1,2}(?:\\.\\d+)?)"));
    const auto timeMatch = timeRe.match(line);
    if (timeMatch.hasMatch()) {
        const qint64 ms = parseTimeStringToMs(timeMatch.captured(1));
        if (ms >= 0) {
            updateProgressFromElapsedTime(ms);
        }
        return true;
    }

    return false;
}

void VideoExporter::updateProgressFromElapsedTime(qint64 elapsedMs)
{
    if (elapsedMs < 0) {
        return;
    }

    m_lastOutTimeMs = elapsedMs;

    if (m_totalDurationMs > 0) {
        // Cap at 99% while running; 100% is set in onProcessFinished()
        int percent = int(elapsedMs * 100 / m_totalDurationMs);
        percent = qMax(1, qMin(percent, 99));
        setProgress(percent);
    } else if (m_progress < 90) {
        // Total duration not known (yet): at least show that we are working
        setProgress(m_progress + 1);
    }
}

qint64 VideoExporter::parseTimeStringToMs(const QString &timeStr) const
{
    // FFmpeg time format: HH:MM:SS or HH:MM:SS.mmm..., e.g. "00:00:02.800000"
    static const QRegularExpression timeRe(
        QStringLiteral("^(\\d+):(\\d{1,2}):(\\d{1,2})(?:\\.(\\d+))?$"));
    const auto match = timeRe.match(timeStr.trimmed());
    if (!match.hasMatch()) {
        return -1;
    }

    qint64 ms = match.captured(1).toLongLong() * 3600000
              + match.captured(2).toLongLong() * 60000
              + match.captured(3).toLongLong() * 1000;

    const QString fraction = match.captured(4);
    if (!fraction.isEmpty()) {
        ms += (fraction + QStringLiteral("000")).left(3).toLongLong();
    }
    return ms;
}

bool VideoExporter::validateInputFile(const QString &mpdPath) const
{
    if (mpdPath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(mpdPath);
    return fileInfo.exists() && fileInfo.isReadable() && fileInfo.suffix().toLower() == "mpd";
}

bool VideoExporter::validateFfmpegExecutable() const
{
    if (m_ffmpegPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(m_ffmpegPath);
    if (info.isAbsolute() || m_ffmpegPath.contains(QLatin1Char('/'))
        || m_ffmpegPath.contains(QLatin1Char('\\'))) {
        return info.isFile() && info.isExecutable();
    }

    // A bare executable name: resolve it through PATH without spawning a
    // process. Spawning ffmpeg and calling waitForFinished() here ran on the
    // GUI thread and froze the window for up to five seconds.
    return !QStandardPaths::findExecutable(m_ffmpegPath).isEmpty();
}

QString VideoExporter::ensureOutputDirectory(const QString &outputPath) const
{
    if (!validateOutputPath(outputPath)) {
        return QString();
    }

    return outputPath;
}

void VideoExporter::addToLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logLine = QString("[%1] %2").arg(timestamp, message);

    // Queue the line; flushPendingLog() applies the batch to m_exportLog.
    // Appending (and notifying QML) for every single FFmpeg output line made
    // the log view re-render constantly and froze the UI on long exports.
    m_pendingLogLines.append(logLine);

    qDebug() << "VideoExporter:" << message;
}

void VideoExporter::flushPendingLog()
{
    if (m_pendingLogLines.isEmpty()) {
        return;
    }

    for (const QString &line : m_pendingLogLines) {
        m_exportLog += line + QLatin1Char('\n');
    }
    m_pendingLogLines.clear();

    // Keep the log bounded so the QML view stays cheap to render no matter
    // how much FFmpeg prints
    static const int MaxLogLines = 500;
    const int lineCount = m_exportLog.count(QLatin1Char('\n'));
    if (lineCount > MaxLogLines) {
        int cutPos = 0;
        for (int i = 0; i < lineCount - MaxLogLines; ++i) {
            cutPos = m_exportLog.indexOf(QLatin1Char('\n'), cutPos) + 1;
        }
        m_exportLog.remove(0, cutPos);
    }

    emit exportLogChanged();
}

void VideoExporter::setCurrentOperation(const QString &operation)
{
    if (m_currentOperation != operation) {
        m_currentOperation = operation;
        emit currentOperationChanged();
    }
}

void VideoExporter::setProgress(int progress)
{
    progress = qBound(0, progress, 100);
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
}

void VideoExporter::setIsExporting(bool exporting)
{
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

void VideoExporter::setLastError(const QString &error)
{
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}

void VideoExporter::resetState()
{
    setIsExporting(false);
    setCurrentOperation(QString());
    setProgress(0);
    setLastError(QString());
    m_currentInputPath.clear();
    m_currentOutputPath.clear();
    m_startTime = 0;
    m_lastProgress = 0;
    m_lastOutTimeMs = 0;
    m_totalDurationMs = 0;
    m_progressBuffer.clear();
}

