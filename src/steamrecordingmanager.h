#ifndef STEAMRECORDINGMANAGER_H
#define STEAMRECORDINGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QAbstractListModel>
#include <QQmlListProperty>

class RecordingClip;
class GameInfo;

// Steam user information structure
struct SteamUser {
    QString steamId3;        // SteamID3 for directory name
    QString steamId64;       // Full STEAMID64
    QString accountName;     // Account name from config
    QString personaName;     // Display name
    QString userDataPath;    // Full path to user data directory
    QString gameRecordingsPath; // Full path to game recordings directory
    bool hasRecordings;      // Whether recordings directory exists
    
    SteamUser() : hasRecordings(false) {}
};

class SteamRecordingManager : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(QString steamPath READ steamPath WRITE setSteamPath NOTIFY steamPathChanged)
    Q_PROPERTY(QString gameRecordingsPath READ gameRecordingsPath NOTIFY gameRecordingsPathChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(int clipCount READ clipCount NOTIFY clipCountChanged)
    Q_PROPERTY(QStringList scanResults READ scanResults NOTIFY scanResultsChanged)
    Q_PROPERTY(bool hasClips READ hasClips NOTIFY hasClipsChanged)
    Q_PROPERTY(QStringList availableUsers READ availableUsers NOTIFY availableUsersChanged)
    Q_PROPERTY(QString selectedUserId READ selectedUserId WRITE setSelectedUserId NOTIFY selectedUserIdChanged)
    Q_PROPERTY(bool hasMultipleUsers READ hasMultipleUsers NOTIFY hasMultipleUsersChanged)

public:
    enum ClipRoles {
        ClipRole = Qt::UserRole + 1,
        AppIdRole,
        GameNameRole,
        ThumbnailPathRole,
        RecordingDateRole,
        SegmentCountRole,
        FormattedDateRole,
        FormattedSizeRole,
        FormattedDurationRole,
        ClipPathRole
    };

    explicit SteamRecordingManager(QObject *parent = nullptr);
    ~SteamRecordingManager() override;

    // Property getters
    QString steamPath() const { return m_steamPath; }
    QString gameRecordingsPath() const { return m_gameRecordingsPath; }
    bool isScanning() const { return m_isScanning; }
    int clipCount() const { return m_clips.size(); }
    QStringList scanResults() const { return m_scanResults; }
    bool hasClips() const { return !m_clips.isEmpty(); }
    QStringList availableUsers() const { return m_availableUsers; }
    QString selectedUserId() const { return m_selectedUserId; }
    bool hasMultipleUsers() const { return m_steamUsers.size() > 1; }

    // Property setters
    void setSteamPath(const QString &steamPath);
    void setSelectedUserId(const QString &userId);

    // QAbstractListModel implementation
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void refreshClips();
    Q_INVOKABLE void clearClips();
    Q_INVOKABLE RecordingClip* getClip(int index) const;
    Q_INVOKABLE QStringList getClipsByGame(const QString &appId) const;
    Q_INVOKABLE int getClipIndex(RecordingClip *clip) const;
    Q_INVOKABLE void refreshGameInfo();
    Q_INVOKABLE QString getUserDisplayName(const QString &userId) const;
    Q_INVOKABLE void refreshAvailableUsers();

public slots:
    void scanForClips();
    void onScanCompleted();

signals:
    void steamPathChanged();
    void gameRecordingsPathChanged();
    void isScanningChanged();
    void clipCountChanged();
    void scanResultsChanged();
    void hasClipsChanged();
    void scanCompleted();
    void clipAdded(RecordingClip *clip);
    void clipRemoved(RecordingClip *clip);
    void availableUsersChanged();
    void selectedUserIdChanged();
    void hasMultipleUsersChanged();

private slots:
    void performScan();
    void loadGameInfoForClips();

private:
    // Helper methods
    void updateGameRecordingsPath();
    void addScanResult(const QString &message);
    void clearScanResults();
    void setIsScanning(bool scanning);
    void addClip(RecordingClip *clip);
    void removeClip(RecordingClip *clip);
    void clearAllClips();
    
    // Game info loading helpers
    void loadGameInfoForClip(RecordingClip *clip);
    GameInfo* loadGameInfo(const QString &appId);
    
    // Validation helpers
    bool validateSteamPath(const QString &path) const;
    QString findGameRecordingsPath(const QString &steamPath) const;
    QString findGameRecordingsPathForUser(const QString &steamPath, const QString &userId) const;
    
    // User management helpers
    void scanForSteamUsers();
    QList<SteamUser> parseSteamUsers(const QString &steamPath) const;
    QString parseSteamConfig(const QString &steamPath) const;
    QString steamId64ToSteamId3(const QString &steamId64) const;

    // Member variables
    QString m_steamPath;
    QString m_gameRecordingsPath;
    bool m_isScanning;
    QList<RecordingClip*> m_clips;
    QStringList m_scanResults;
    QTimer *m_scanTimer;
    
    // User management
    QList<SteamUser> m_steamUsers;
    QStringList m_availableUsers;
    QString m_selectedUserId;
    
    // Game info cache
    QHash<QString, GameInfo*> m_gameInfoCache;
    
    // Scanning state
    bool m_scanRequested;
    bool m_gameInfoLoadRequested;
};

#endif // STEAMRECORDINGMANAGER_H