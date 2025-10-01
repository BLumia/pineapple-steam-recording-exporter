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

class SteamRecordingManager : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(QString steamPath READ steamPath WRITE setSteamPath NOTIFY steamPathChanged)
    Q_PROPERTY(QString gameRecordingsPath READ gameRecordingsPath NOTIFY gameRecordingsPathChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(int clipCount READ clipCount NOTIFY clipCountChanged)
    Q_PROPERTY(QStringList scanResults READ scanResults NOTIFY scanResultsChanged)
    Q_PROPERTY(bool hasClips READ hasClips NOTIFY hasClipsChanged)

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

    // Property setters
    void setSteamPath(const QString &steamPath);

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

    // Member variables
    QString m_steamPath;
    QString m_gameRecordingsPath;
    bool m_isScanning;
    QList<RecordingClip*> m_clips;
    QStringList m_scanResults;
    QTimer *m_scanTimer;
    
    // Game info cache
    QHash<QString, GameInfo*> m_gameInfoCache;
    
    // Scanning state
    bool m_scanRequested;
    bool m_gameInfoLoadRequested;
};

#endif // STEAMRECORDINGMANAGER_H