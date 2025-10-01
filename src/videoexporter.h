#ifndef VIDEOEXPORTER_H
#define VIDEOEXPORTER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QUrl>

class RecordingClip;

class VideoExporter : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(QString currentOperation READ currentOperation NOTIFY currentOperationChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString exportLog READ exportLog NOTIFY exportLogChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString defaultExportPath READ defaultExportPath WRITE setDefaultExportPath NOTIFY defaultExportPathChanged)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath WRITE setFfmpegPath NOTIFY ffmpegPathChanged)

public:
    explicit VideoExporter(QObject *parent = nullptr);
    ~VideoExporter() override;

    // Property getters
    bool isExporting() const { return m_isExporting; }
    QString currentOperation() const { return m_currentOperation; }
    int progress() const { return m_progress; }
    QString exportLog() const { return m_exportLog; }
    QString lastError() const { return m_lastError; }
    QString defaultExportPath() const { return m_defaultExportPath; }
    QString ffmpegPath() const { return m_ffmpegPath; }

    // Property setters
    void setDefaultExportPath(const QString &path);
    void setFfmpegPath(const QString &path);

    // Export methods
    Q_INVOKABLE bool exportClipSegment(RecordingClip *clip, int segmentIndex, const QString &outputPath = QString());
    Q_INVOKABLE bool exportEntireClip(RecordingClip *clip, const QString &outputPath = QString());
    Q_INVOKABLE void cancelExport();
    
    // Utility methods
    Q_INVOKABLE QString generateOutputFilename(RecordingClip *clip, int segmentIndex = -1) const;
    Q_INVOKABLE QString selectOutputPath(const QString &suggestedName = QString()) const;
    Q_INVOKABLE bool validateOutputPath(const QString &path) const;
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE void openExportLocation(const QString &filePath) const;

    // Static utility methods
    static QString getDefaultExportDirectory();
    static QString sanitizeFilename(const QString &filename);

signals:
    void isExportingChanged();
    void currentOperationChanged();
    void progressChanged();
    void exportLogChanged();
    void lastErrorChanged();
    void defaultExportPathChanged();
    void ffmpegPathChanged();
    void exportStarted(const QString &outputPath);
    void exportCompleted(const QString &outputPath);
    void exportFailed(const QString &error);
    void exportCancelled();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void updateProgress();

private:
    // Export implementation
    bool startExport(const QString &inputMpdPath, const QString &outputPath, const QString &operationName);
    void setupExportProcess(const QString &inputMpdPath, const QString &outputPath);
    QStringList buildFfmpegArguments(const QString &inputMpdPath, const QString &outputPath) const;
    
    // Progress parsing
    void parseProgressFromOutput(const QString &output);
    int extractProgressPercentage(const QString &line) const;
    
    // Validation and helpers
    bool validateInputFile(const QString &mpdPath) const;
    bool validateFfmpegExecutable() const;
    QString ensureOutputDirectory(const QString &outputPath) const;
    void addToLog(const QString &message);
    void setCurrentOperation(const QString &operation);
    void setProgress(int progress);
    void setIsExporting(bool exporting);
    void setLastError(const QString &error);
    void resetState();
    QString formatFileSize(qint64 bytes) const;

    // Member variables
    bool m_isExporting;
    QString m_currentOperation;
    int m_progress;
    QString m_exportLog;
    QString m_lastError;
    QString m_defaultExportPath;
    QString m_ffmpegPath;
    
    // Process management
    QProcess *m_ffmpegProcess;
    QTimer *m_progressTimer;
    QString m_currentOutputPath;
    QString m_currentInputPath;
    
    // Progress tracking
    qint64 m_startTime;
    int m_lastProgress;
    QString m_progressBuffer;
};

#endif // VIDEOEXPORTER_H