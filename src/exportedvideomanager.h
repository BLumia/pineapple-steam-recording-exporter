#ifndef EXPORTEDVIDEOMANAGER_H
#define EXPORTEDVIDEOMANAGER_H

#include <QObject>
#include <QAbstractListModel>
#include <QString>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QUrl>

class ExportedVideo : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QString fileName READ fileName CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(qint64 fileSize READ fileSize CONSTANT)
    Q_PROPERTY(QString fileSizeString READ fileSizeString CONSTANT)
    Q_PROPERTY(QDateTime creationTime READ creationTime CONSTANT)
    Q_PROPERTY(QString creationTimeString READ creationTimeString CONSTANT)
    Q_PROPERTY(QUrl fileUrl READ fileUrl CONSTANT)
    Q_PROPERTY(QString thumbnailPath READ thumbnailPath CONSTANT)

public:
    explicit ExportedVideo(const QString &filePath, QObject *parent = nullptr);

    QString filePath() const { return m_filePath; }
    QString fileName() const { return m_fileName; }
    QString displayName() const { return m_displayName; }
    qint64 fileSize() const { return m_fileSize; }
    QString fileSizeString() const { return m_fileSizeString; }
    QDateTime creationTime() const { return m_creationTime; }
    QString creationTimeString() const { return m_creationTimeString; }
    QUrl fileUrl() const { return m_fileUrl; }
    QString thumbnailPath() const { return m_thumbnailPath; }

    // Utility methods
    Q_INVOKABLE bool deleteVideo();
    Q_INVOKABLE void openInExplorer() const;
    Q_INVOKABLE void playWithDefaultPlayer() const;

signals:
    void videoDeleted();

private:
    void updateFileInfo();
    QString extractDisplayName() const;

    QString m_filePath;
    QString m_fileName;
    QString m_displayName;
    qint64 m_fileSize;
    QString m_fileSizeString;
    QDateTime m_creationTime;
    QString m_creationTimeString;
    QUrl m_fileUrl;
    QString m_thumbnailPath;
};

class ExportedVideoManager : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(int videoCount READ videoCount NOTIFY videoCountChanged)
    Q_PROPERTY(QString exportDirectory READ exportDirectory WRITE setExportDirectory NOTIFY exportDirectoryChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY totalSizeChanged)
    Q_PROPERTY(QString totalSizeString READ totalSizeString NOTIFY totalSizeStringChanged)

public:
    enum VideoRoles {
        VideoRole = Qt::UserRole + 1,
        FilePathRole,
        FileNameRole,
        DisplayNameRole,
        FileSizeRole,
        FileSizeStringRole,
        CreationTimeRole,
        CreationTimeStringRole,
        FileUrlRole,
        ThumbnailPathRole
    };

    explicit ExportedVideoManager(QObject *parent = nullptr);
    ~ExportedVideoManager() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Property getters
    int videoCount() const { return m_videos.size(); }
    QString exportDirectory() const { return m_exportDirectory; }
    bool isScanning() const { return m_isScanning; }
    QString lastError() const { return m_lastError; }
    qint64 totalSize() const { return m_totalSize; }
    QString totalSizeString() const { return m_totalSizeString; }

    // Property setters
    void setExportDirectory(const QString &directory);

    // Public methods
    Q_INVOKABLE void refreshVideos();
    Q_INVOKABLE void deleteVideo(int index);
    Q_INVOKABLE void deleteAllVideos();
    Q_INVOKABLE ExportedVideo* getVideo(int index) const;
    Q_INVOKABLE void openExportDirectory() const;
    Q_INVOKABLE void sortByName();
    Q_INVOKABLE void sortByDate();
    Q_INVOKABLE void sortBySize();

signals:
    void videoCountChanged();
    void exportDirectoryChanged();
    void isScanningChanged();
    void lastErrorChanged();
    void totalSizeChanged();
    void totalSizeStringChanged();
    void videoAdded(ExportedVideo *video);
    void videoRemoved(int index);
    void scanCompleted();
    void scanFailed(const QString &error);

private slots:
    void onDirectoryChanged(const QString &path);
    void onVideoDeleted();

private:
    void scanDirectory();
    void addVideo(const QString &filePath);
    void removeVideo(int index);
    void clearVideos();
    void updateTotalSize();
    void setIsScanning(bool scanning);
    void setLastError(const QString &error);
    bool isVideoFile(const QString &filePath) const;

    QList<ExportedVideo*> m_videos;
    QString m_exportDirectory;
    bool m_isScanning;
    QString m_lastError;
    qint64 m_totalSize;
    QString m_totalSizeString;
    QFileSystemWatcher *m_watcher;
};

#endif // EXPORTEDVIDEOMANAGER_H