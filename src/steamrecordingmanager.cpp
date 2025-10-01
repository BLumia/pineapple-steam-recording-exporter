#include "steamrecordingmanager.h"
#include "recordingclip.h"
#include "gameinfo.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QTimer>
// Removed QtConcurrent dependency - not available in this Qt configuration
// #include <QtConcurrent>
// #include <QFuture>
// #include <QFutureWatcher>

SteamRecordingManager::SteamRecordingManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_isScanning(false)
    , m_scanTimer(new QTimer(this))
    , m_scanRequested(false)
    , m_gameInfoLoadRequested(false)
{
    m_scanTimer->setSingleShot(true);
    m_scanTimer->setInterval(100);
    connect(m_scanTimer, &QTimer::timeout, this, &SteamRecordingManager::performScan);
}

SteamRecordingManager::~SteamRecordingManager()
{
    clearAllClips();
    qDeleteAll(m_gameInfoCache);
}

void SteamRecordingManager::setSteamPath(const QString &steamPath)
{
    if (m_steamPath != steamPath) {
        m_steamPath = steamPath;
        emit steamPathChanged();
        
        updateGameRecordingsPath();
        clearAllClips();
        clearScanResults();
        
        // Clear game info cache when steam path changes
        qDeleteAll(m_gameInfoCache);
        m_gameInfoCache.clear();
    }
}

int SteamRecordingManager::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_clips.size();
}

QVariant SteamRecordingManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_clips.size()) {
        return QVariant();
    }

    RecordingClip *clip = m_clips.at(index.row());
    if (!clip) {
        return QVariant();
    }

    switch (role) {
    case ClipRole:
        return QVariant::fromValue(clip);
    case AppIdRole:
        return clip->appId();
    case GameNameRole:
        return clip->gameName();
    case ThumbnailPathRole:
        return clip->thumbnailPath();
    case RecordingDateRole:
        return clip->recordingDate();
    case SegmentCountRole:
        return clip->segmentCount();
    case FormattedDateRole:
        return clip->formattedDate();
    case FormattedSizeRole:
        return clip->formattedSize();
    case FormattedDurationRole:
        return clip->formattedDuration();
    case ClipPathRole:
        return clip->clipPath();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SteamRecordingManager::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ClipRole] = "clip";
    roles[AppIdRole] = "appId";
    roles[GameNameRole] = "gameName";
    roles[ThumbnailPathRole] = "thumbnailPath";
    roles[RecordingDateRole] = "recordingDate";
    roles[SegmentCountRole] = "segmentCount";
    roles[FormattedDateRole] = "formattedDate";
    roles[FormattedSizeRole] = "formattedSize";
    roles[FormattedDurationRole] = "formattedDuration";
    roles[ClipPathRole] = "clipPath";
    return roles;
}

void SteamRecordingManager::startScan()
{
    if (m_isScanning) {
        addScanResult(tr("Scan already in progress..."));
        return;
    }

    if (m_steamPath.isEmpty()) {
        addScanResult(tr("Error: Steam path not set"));
        return;
    }

    if (m_gameRecordingsPath.isEmpty()) {
        addScanResult(tr("Error: Game recordings path not found"));
        return;
    }

    addScanResult(tr("Starting scan for recording clips..."));
    m_scanRequested = true;
    m_scanTimer->start();
}

void SteamRecordingManager::refreshClips()
{
    if (!m_isScanning) {
        startScan();
    }
}

void SteamRecordingManager::clearClips()
{
    clearAllClips();
    clearScanResults();
}

RecordingClip* SteamRecordingManager::getClip(int index) const
{
    if (index < 0 || index >= m_clips.size()) {
        return nullptr;
    }
    return m_clips.at(index);
}

QStringList SteamRecordingManager::getClipsByGame(const QString &appId) const
{
    QStringList clipPaths;
    
    for (RecordingClip *clip : m_clips) {
        if (clip && clip->appId() == appId) {
            clipPaths.append(clip->clipPath());
        }
    }
    
    return clipPaths;
}

int SteamRecordingManager::getClipIndex(RecordingClip *clip) const
{
    return m_clips.indexOf(clip);
}

void SteamRecordingManager::refreshGameInfo()
{
    if (m_clips.isEmpty()) {
        return;
    }

    m_gameInfoLoadRequested = true;
    
    // Load game info in background
    QTimer::singleShot(0, this, &SteamRecordingManager::loadGameInfoForClips);
}

void SteamRecordingManager::scanForClips()
{
    startScan();
}

void SteamRecordingManager::onScanCompleted()
{
    addScanResult(tr("Scan completed. Found %1 clips.").arg(m_clips.size()));
    
    // Start loading game info for the found clips
    if (!m_clips.isEmpty()) {
        refreshGameInfo();
    }
    
    emit scanCompleted();
}

void SteamRecordingManager::performScan()
{
    if (!m_scanRequested) {
        return;
    }

    m_scanRequested = false;
    setIsScanning(true);
    
    addScanResult(tr("Scanning directory: %1").arg(m_gameRecordingsPath));

    // Clear existing clips
    clearAllClips();

    // Find all clips
    QList<RecordingClip*> foundClips = RecordingClip::findAllClips(m_gameRecordingsPath, this);
    
    if (foundClips.isEmpty()) {
        addScanResult(tr("No recording clips found."));
    } else {
        addScanResult(tr("Found %1 recording clips").arg(foundClips.size()));
        
        // Add clips to the model
        beginInsertRows(QModelIndex(), 0, foundClips.size() - 1);
        for (RecordingClip *clip : foundClips) {
            m_clips.append(clip);
            
            // Connect clip signals for live updates
            connect(clip, &RecordingClip::gameNameChanged, this, [this, clip]() {
                int index = getClipIndex(clip);
                if (index >= 0) {
                    QModelIndex modelIndex = createIndex(index, 0);
                    emit dataChanged(modelIndex, modelIndex, {GameNameRole});
                }
            });
            
            emit clipAdded(clip);
        }
        endInsertRows();
        
        emit clipCountChanged();
        emit hasClipsChanged();
    }

    setIsScanning(false);
    onScanCompleted();
}

void SteamRecordingManager::loadGameInfoForClips()
{
    if (!m_gameInfoLoadRequested || m_clips.isEmpty()) {
        return;
    }

    m_gameInfoLoadRequested = false;
    addScanResult(tr("Loading game information..."));

    int loadedCount = 0;
    for (RecordingClip *clip : m_clips) {
        if (clip && !clip->appId().isEmpty()) {
            loadGameInfoForClip(clip);
            if (!clip->gameName().isEmpty()) {
                loadedCount++;
            }
        }
    }

    if (loadedCount > 0) {
        addScanResult(tr("Loaded game information for %1 clips").arg(loadedCount));
    } else {
        addScanResult(tr("No game information could be loaded"));
    }
}

void SteamRecordingManager::updateGameRecordingsPath()
{
    QString newPath = findGameRecordingsPath(m_steamPath);
    if (m_gameRecordingsPath != newPath) {
        m_gameRecordingsPath = newPath;
        emit gameRecordingsPathChanged();
    }
}

void SteamRecordingManager::addScanResult(const QString &message)
{
    m_scanResults.append(message);
    emit scanResultsChanged();
    qDebug() << "SteamRecordingManager:" << message;
}

void SteamRecordingManager::clearScanResults()
{
    if (!m_scanResults.isEmpty()) {
        m_scanResults.clear();
        emit scanResultsChanged();
    }
}

void SteamRecordingManager::setIsScanning(bool scanning)
{
    if (m_isScanning != scanning) {
        m_isScanning = scanning;
        emit isScanningChanged();
    }
}

void SteamRecordingManager::addClip(RecordingClip *clip)
{
    if (!clip) {
        return;
    }

    beginInsertRows(QModelIndex(), m_clips.size(), m_clips.size());
    m_clips.append(clip);
    endInsertRows();

    emit clipCountChanged();
    emit hasClipsChanged();
    emit clipAdded(clip);
}

void SteamRecordingManager::removeClip(RecordingClip *clip)
{
    if (!clip) {
        return;
    }

    int index = m_clips.indexOf(clip);
    if (index >= 0) {
        beginRemoveRows(QModelIndex(), index, index);
        m_clips.removeAt(index);
        endRemoveRows();

        emit clipCountChanged();
        emit hasClipsChanged();
        emit clipRemoved(clip);
        
        clip->deleteLater();
    }
}

void SteamRecordingManager::clearAllClips()
{
    if (!m_clips.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_clips.size() - 1);
        qDeleteAll(m_clips);
        m_clips.clear();
        endRemoveRows();

        emit clipCountChanged();
        emit hasClipsChanged();
    }
}

void SteamRecordingManager::loadGameInfoForClip(RecordingClip *clip)
{
    if (!clip || clip->appId().isEmpty()) {
        return;
    }

    // Check cache first
    GameInfo *gameInfo = m_gameInfoCache.value(clip->appId(), nullptr);
    if (!gameInfo) {
        gameInfo = loadGameInfo(clip->appId());
        if (gameInfo) {
            m_gameInfoCache.insert(clip->appId(), gameInfo);
        }
    }

    if (gameInfo && gameInfo->isValid()) {
        clip->refreshGameInfo(m_steamPath);
    }
}

GameInfo* SteamRecordingManager::loadGameInfo(const QString &appId)
{
    if (appId.isEmpty() || m_steamPath.isEmpty()) {
        return nullptr;
    }

    GameInfo *gameInfo = GameInfo::fromAppId(appId, m_steamPath, this);
    if (!gameInfo || !gameInfo->isValid()) {
        qDebug() << "Failed to load game info for AppID:" << appId;
        if (gameInfo) {
            gameInfo->deleteLater();
        }
        return nullptr;
    }

    qDebug() << "Loaded game info:" << gameInfo->name() << "(AppID:" << appId << ")";
    return gameInfo;
}

bool SteamRecordingManager::validateSteamPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    QDir steamDir(path);
    return steamDir.exists() && 
           steamDir.exists("userdata") && 
           steamDir.exists("steamapps");
}

QString SteamRecordingManager::findGameRecordingsPath(const QString &steamPath) const
{
    if (!validateSteamPath(steamPath)) {
        return QString();
    }

    QDir steamDir(steamPath);
    QDir userDataDir = steamDir;
    
    if (!userDataDir.cd("userdata")) {
        qWarning() << "userdata directory not found in Steam path:" << steamPath;
        return QString();
    }

    // Find the first user directory (could be enhanced to handle multiple users)
    QStringList userDirs = userDataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (userDirs.isEmpty()) {
        qWarning() << "No user directories found in userdata";
        return QString();
    }

    // Use the first user directory found
    QString userId = userDirs.first();
    QString gameRecordingsPath = userDataDir.absoluteFilePath(userId + "/gamerecordings");
    
    QDir gameRecordingsDir(gameRecordingsPath);
    if (!gameRecordingsDir.exists()) {
        qDebug() << "Game recordings directory does not exist:" << gameRecordingsPath;
        return QString(); // Return empty string but don't warn - user might not have recordings yet
    }

    return gameRecordingsPath;
}