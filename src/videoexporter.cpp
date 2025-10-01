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

VideoExporter::VideoExporter(QObject *parent)
    : QObject(parent)
    , m_isExporting(false)
    , m_progress(0)
    , m_ffmpegProcess(new QProcess(this))
    , m_progressTimer(new QTimer(this))
    , m_startTime(0)
    , m_lastProgress(0)
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
    
    m_ffmpegProcess->kill();
    if (!m_ffmpegProcess->waitForFinished(3000)) {
        m_ffmpegProcess->terminate();
        m_ffmpegProcess->waitForFinished(1000);
    }

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
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString exportPath = QDir(documentsPath).absoluteFilePath("Steam Recording Exports");
    
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
    
    if (exitStatus == QProcess::CrashExit) {
        QString error = tr("FFmpeg process crashed");
        setLastError(error);
        addToLog(error);
        resetState();
        emit exportFailed(error);
        return;
    }

    if (exitCode == 0) {
        // Success
        setProgress(100);
        addToLog(tr("Export completed successfully!"));
        
        QFileInfo outputFile(m_currentOutputPath);
        if (outputFile.exists()) {
            addToLog(tr("Output file: %1 (%2)")
                .arg(outputFile.fileName())
                .arg(this->formatFileSize(outputFile.size())));
        }
        
        QString outputPath = m_currentOutputPath;
        resetState();
        emit exportCompleted(outputPath);
    } else {
        // Error
        QString error = tr("FFmpeg exited with code %1").arg(exitCode);
        setLastError(error);
        addToLog(error);
        resetState();
        emit exportFailed(error);
    }
}

void VideoExporter::onProcessError(QProcess::ProcessError error)
{
    m_progressTimer->stop();
    
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
    resetState();
    emit exportFailed(errorString);
}

void VideoExporter::onProcessReadyReadStandardOutput()
{
    if (!m_ffmpegProcess) {
        return;
    }

    QByteArray data = m_ffmpegProcess->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    
    m_progressBuffer += output;
    parseProgressFromOutput(output);
    
    // Add to log (but filter out too verbose output)
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.contains("frame=") && !line.contains("fps=")) {
            addToLog(line.trimmed());
        }
    }
}

void VideoExporter::onProcessReadyReadStandardError()
{
    if (!m_ffmpegProcess) {
        return;
    }

    QByteArray data = m_ffmpegProcess->readAllStandardError();
    QString output = QString::fromUtf8(data);
    
    m_progressBuffer += output;
    parseProgressFromOutput(output);
    
    // FFmpeg sends progress info to stderr, so we need to parse it
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.contains("time=") || line.contains("frame=") || line.contains("Error") || line.contains("Warning")) {
            addToLog(line.trimmed());
        }
    }
}

void VideoExporter::updateProgress()
{
    // This is called periodically during export to update UI
    // Progress is mainly updated from parsing FFmpeg output
    emit progressChanged();
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
    
    // Copy streams without re-encoding for faster, lossless conversion
    args << "-c" << "copy";
    
    // Overwrite output file without prompting
    args << "-y";
    
    // Progress reporting
    args << "-progress" << "pipe:2";
    
    // Output file
    args << outputPath;
    
    return args;
}

void VideoExporter::parseProgressFromOutput(const QString &output)
{
    // FFmpeg progress format includes lines like:
    // frame=1234
    // fps=30.5
    // time=00:01:23.45
    // This is a basic implementation - could be improved with more sophisticated parsing
    
    QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        if (line.startsWith("time=")) {
            // Extract time and estimate progress
            QString timeStr = line.mid(5).trimmed();
            // This would need duration information to calculate accurate percentage
            // For now, just show that progress is being made
            if (m_progress < 90) {
                setProgress(m_progress + 1);
            }
        }
    }
}

int VideoExporter::extractProgressPercentage(const QString &line) const
{
    // This would extract percentage from FFmpeg output
    // Implementation depends on specific FFmpeg output format
    Q_UNUSED(line)
    return 0;
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
    QProcess testProcess;
    testProcess.start(m_ffmpegPath, QStringList() << "-version");
    testProcess.waitForFinished(5000);
    
    return testProcess.exitCode() == 0;
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
    
    m_exportLog += logLine + "\n";
    emit exportLogChanged();
    
    qDebug() << "VideoExporter:" << message;
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
    m_progressBuffer.clear();
}

QString VideoExporter::formatFileSize(qint64 bytes) const
{
    if (bytes <= 0) {
        return tr("0 bytes");
    }

    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;

    if (bytes >= gb) {
        return tr("%1 GB").arg(QString::number(static_cast<double>(bytes) / gb, 'f', 2));
    } else if (bytes >= mb) {
        return tr("%1 MB").arg(QString::number(static_cast<double>(bytes) / mb, 'f', 1));
    } else if (bytes >= kb) {
        return tr("%1 KB").arg(QString::number(static_cast<double>(bytes) / kb, 'f', 1));
    } else {
        return tr("%1 bytes").arg(bytes);
    }
}