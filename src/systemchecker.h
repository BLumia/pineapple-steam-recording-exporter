#ifndef SYSTEMCHECKER_H
#define SYSTEMCHECKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class SystemChecker : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isChecking READ isChecking NOTIFY isCheckingChanged)
    Q_PROPERTY(bool steamFound READ steamFound NOTIFY steamFoundChanged)
    Q_PROPERTY(bool ffmpegFound READ ffmpegFound NOTIFY ffmpegFoundChanged)
    Q_PROPERTY(QString steamPath READ steamPath NOTIFY steamPathChanged)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath NOTIFY ffmpegPathChanged)
    Q_PROPERTY(QStringList checkResults READ checkResults NOTIFY checkResultsChanged)
    Q_PROPERTY(bool allChecksPass READ allChecksPass NOTIFY allChecksPassChanged)

public:
    explicit SystemChecker(QObject *parent = nullptr);

    // Property getters
    bool isChecking() const { return m_isChecking; }
    bool steamFound() const { return m_steamFound; }
    bool ffmpegFound() const { return m_ffmpegFound; }
    QString steamPath() const { return m_steamPath; }
    QString ffmpegPath() const { return m_ffmpegPath; }
    QStringList checkResults() const { return m_checkResults; }
    bool allChecksPass() const { return m_steamFound && m_ffmpegFound; }

public slots:
    void startSystemCheck();
    void recheckSteam();
    void recheckFfmpeg();

signals:
    void isCheckingChanged();
    void steamFoundChanged();
    void ffmpegFoundChanged();
    void steamPathChanged();
    void ffmpegPathChanged();
    void checkResultsChanged();
    void allChecksPassChanged();
    void systemCheckCompleted();

private slots:
    void performChecks();

private:
    // Steam detection methods
    QString findSteamPathWindows();
    QString findSteamPathLinux();
    bool validateSteamPath(const QString &path);
    QString getGameRecordingsPath(const QString &steamPath);

    // FFmpeg detection methods
    QString findFfmpegInPath();
    bool validateFfmpeg(const QString &ffmpegPath);

    // Helper methods
    void addCheckResult(const QString &message);
    void clearCheckResults();
    void setIsChecking(bool checking);
    void setSteamFound(bool found);
    void setFfmpegFound(bool found);
    void setSteamPath(const QString &path);
    void setFfmpegPath(const QString &path);

    // Member variables
    bool m_isChecking;
    bool m_steamFound;
    bool m_ffmpegFound;
    QString m_steamPath;
    QString m_ffmpegPath;
    QString m_gameRecordingsPath;
    QStringList m_checkResults;
    QTimer *m_checkTimer;
};

#endif // SYSTEMCHECKER_H