#include "systemchecker.h"
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QWinEventNotifier>
#endif

SystemChecker::SystemChecker(QObject *parent)
    : QObject(parent)
    , m_isChecking(false)
    , m_steamFound(false)
    , m_ffmpegFound(false)
    , m_checkTimer(new QTimer(this))
{
    m_checkTimer->setSingleShot(true);
    connect(m_checkTimer, &QTimer::timeout, this, &SystemChecker::performChecks);
}

void SystemChecker::startSystemCheck()
{
    if (m_isChecking) {
        return;
    }

    setIsChecking(true);
    clearCheckResults();
    addCheckResult(tr("Starting system check..."));

    // Start checks with a small delay to allow UI to update
    m_checkTimer->start(100);
}

void SystemChecker::recheckSteam()
{
    addCheckResult(tr("Rechecking Steam installation..."));
    
    QString steamPath;
#ifdef Q_OS_WIN
    steamPath = findSteamPathWindows();
#else
    steamPath = findSteamPathLinux();
#endif

    if (!steamPath.isEmpty() && validateSteamPath(steamPath)) {
        setSteamPath(steamPath);
        setSteamFound(true);
        m_gameRecordingsPath = getGameRecordingsPath(steamPath);
        addCheckResult(tr("✓ Steam found at: %1").arg(steamPath));
        addCheckResult(tr("✓ Game recordings path: %1").arg(m_gameRecordingsPath));
    } else {
        setSteamFound(false);
        setSteamPath(QString());
        addCheckResult(tr("✗ Steam installation not found or invalid"));
    }

    emit allChecksPassChanged();
}

void SystemChecker::recheckFfmpeg()
{
    addCheckResult(tr("Rechecking FFmpeg..."));
    
    QString ffmpegPath = findFfmpegInPath();
    
    if (!ffmpegPath.isEmpty() && validateFfmpeg(ffmpegPath)) {
        setFfmpegPath(ffmpegPath);
        setFfmpegFound(true);
        addCheckResult(tr("✓ FFmpeg found at: %1").arg(ffmpegPath));
    } else {
        setFfmpegFound(false);
        setFfmpegPath(QString());
        addCheckResult(tr("✗ FFmpeg not found in PATH"));
        addCheckResult(tr("Please install FFmpeg and add it to your system PATH"));
    }

    emit allChecksPassChanged();
}

void SystemChecker::performChecks()
{
    addCheckResult(tr("Checking Steam installation..."));

    // Check Steam
    QString steamPath;
#ifdef Q_OS_WIN
    steamPath = findSteamPathWindows();
#else
    steamPath = findSteamPathLinux();
#endif

    if (!steamPath.isEmpty() && validateSteamPath(steamPath)) {
        setSteamPath(steamPath);
        setSteamFound(true);
        m_gameRecordingsPath = getGameRecordingsPath(steamPath);
        addCheckResult(tr("✓ Steam found at: %1").arg(steamPath));
        addCheckResult(tr("✓ Game recordings path: %1").arg(m_gameRecordingsPath));
    } else {
        setSteamFound(false);
        addCheckResult(tr("✗ Steam installation not found"));
        addCheckResult(tr("Please ensure Steam is installed and try again"));
    }

    addCheckResult(tr("Checking FFmpeg..."));

    // Check FFmpeg
    QString ffmpegPath = findFfmpegInPath();
    
    if (!ffmpegPath.isEmpty() && validateFfmpeg(ffmpegPath)) {
        setFfmpegPath(ffmpegPath);
        setFfmpegFound(true);
        addCheckResult(tr("✓ FFmpeg found at: %1").arg(ffmpegPath));
    } else {
        setFfmpegFound(false);
        addCheckResult(tr("✗ FFmpeg not found in PATH"));
        addCheckResult(tr("Please install FFmpeg and add it to your system PATH"));
        addCheckResult(tr("You can download FFmpeg from: https://ffmpeg.org/download.html"));
    }

    // Final results
    if (allChecksPass()) {
        addCheckResult(tr("✓ All system checks passed! Ready to proceed."));
    } else {
        addCheckResult(tr("⚠ Some checks failed. Please address the issues above."));
    }

    setIsChecking(false);
    emit systemCheckCompleted();
}

QString SystemChecker::findSteamPathWindows()
{
#ifdef Q_OS_WIN
    // Try to read Steam path from registry
    QSettings steamRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Valve\\Steam", QSettings::NativeFormat);
    QString installPath = steamRegistry.value("InstallPath", "").toString();
    
    if (!installPath.isEmpty()) {
        return QDir::toNativeSeparators(installPath);
    }

    // Try 64-bit registry path
    QSettings steamRegistry64("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam", QSettings::NativeFormat);
    installPath = steamRegistry64.value("InstallPath", "").toString();
    
    if (!installPath.isEmpty()) {
        return QDir::toNativeSeparators(installPath);
    }

    // Try common installation paths
    QStringList commonPaths = {
        "C:/Program Files (x86)/Steam",
        "C:/Program Files/Steam",
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/Steam"
    };

    for (const QString &path : commonPaths) {
        if (validateSteamPath(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
#endif
    return QString();
}

QString SystemChecker::findSteamPathLinux()
{
#ifndef Q_OS_WIN
    // Check common Steam installation paths on Linux
    QStringList commonPaths = {
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.steam/steam",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Steam",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
        "/usr/share/steam",
        "/opt/steam"
    };

    for (const QString &path : commonPaths) {
        if (validateSteamPath(path)) {
            return path;
        }
    }

    // Try to find steam executable and derive path
    QProcess process;
    process.start("which", QStringList() << "steam");
    process.waitForFinished(3000);
    
    if (process.exitCode() == 0) {
        QString steamExePath = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        QFileInfo steamExe(steamExePath);
        QString potentialPath = steamExe.dir().absolutePath();
        
        if (validateSteamPath(potentialPath)) {
            return potentialPath;
        }
    }
#endif
    return QString();
}

bool SystemChecker::validateSteamPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    QDir steamDir(path);
    if (!steamDir.exists()) {
        return false;
    }

    // Check for essential Steam files/directories
    QStringList requiredItems = {
        "userdata",
        "steamapps"
    };

#ifdef Q_OS_WIN
    requiredItems << "steam.exe";
#else
    requiredItems << "steam.sh";
#endif

    for (const QString &item : requiredItems) {
        QString itemPath = steamDir.absoluteFilePath(item);
        if (!QFileInfo::exists(itemPath)) {
            qDebug() << "Steam validation failed: missing" << itemPath;
            return false;
        }
    }

    return true;
}

QString SystemChecker::getGameRecordingsPath(const QString &steamPath)
{
    QDir steamDir(steamPath);
    QString userDataPath = steamDir.absoluteFilePath("userdata");
    
    QDir userDataDir(userDataPath);
    QStringList userDirs = userDataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (!userDirs.isEmpty()) {
        // Use the first user directory found (could be enhanced to handle multiple users)
        QString firstUserId = userDirs.first();
        QString gameRecordingsPath = userDataDir.absoluteFilePath(firstUserId + "/gamerecordings");
        return gameRecordingsPath;
    }

    return QString();
}

QString SystemChecker::findFfmpegInPath()
{
    QProcess process;
    QString command;
    
#ifdef Q_OS_WIN
    command = "where";
#else
    command = "which";
#endif

    process.start(command, QStringList() << "ffmpeg");
    process.waitForFinished(5000);

    if (process.exitCode() == 0) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        QStringList paths = output.split('\n');
        if (!paths.isEmpty()) {
            return paths.first().trimmed();
        }
    }

    return QString();
}

bool SystemChecker::validateFfmpeg(const QString &ffmpegPath)
{
    if (ffmpegPath.isEmpty()) {
        return false;
    }

    QProcess process;
    process.start(ffmpegPath, QStringList() << "-version");
    process.waitForFinished(5000);

    return process.exitCode() == 0;
}

void SystemChecker::addCheckResult(const QString &message)
{
    m_checkResults.append(message);
    emit checkResultsChanged();
    qDebug() << "SystemChecker:" << message;
}

void SystemChecker::clearCheckResults()
{
    m_checkResults.clear();
    emit checkResultsChanged();
}

void SystemChecker::setIsChecking(bool checking)
{
    if (m_isChecking != checking) {
        m_isChecking = checking;
        emit isCheckingChanged();
    }
}

void SystemChecker::setSteamFound(bool found)
{
    if (m_steamFound != found) {
        m_steamFound = found;
        emit steamFoundChanged();
    }
}

void SystemChecker::setFfmpegFound(bool found)
{
    if (m_ffmpegFound != found) {
        m_ffmpegFound = found;
        emit ffmpegFoundChanged();
    }
}

void SystemChecker::setSteamPath(const QString &path)
{
    if (m_steamPath != path) {
        m_steamPath = path;
        emit steamPathChanged();
    }
}

void SystemChecker::setFfmpegPath(const QString &path)
{
    if (m_ffmpegPath != path) {
        m_ffmpegPath = path;
        emit ffmpegPathChanged();
    }
}
