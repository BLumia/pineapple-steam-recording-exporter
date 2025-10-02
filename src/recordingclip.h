#ifndef RECORDINGCLIP_H
#define RECORDINGCLIP_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QUrl>
#include <QFileInfo>

class GameInfo;

class RecordingClip : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString appId READ appId WRITE setAppId NOTIFY appIdChanged)
    Q_PROPERTY(QString clipPath READ clipPath WRITE setClipPath NOTIFY clipPathChanged)
    Q_PROPERTY(QString thumbnailPath READ thumbnailPath NOTIFY thumbnailPathChanged)
    Q_PROPERTY(QDateTime recordingDate READ recordingDate NOTIFY recordingDateChanged)
    Q_PROPERTY(QStringList segments READ segments NOTIFY segmentsChanged)
    Q_PROPERTY(int segmentCount READ segmentCount NOTIFY segmentCountChanged)
    Q_PROPERTY(QString gameName READ gameName NOTIFY gameNameChanged)
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY totalSizeChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    Q_PROPERTY(QString formattedDate READ formattedDate NOTIFY formattedDateChanged)
    Q_PROPERTY(QString formattedSize READ formattedSize NOTIFY formattedSizeChanged)
    Q_PROPERTY(QString formattedDuration READ formattedDuration NOTIFY formattedDurationChanged)

public:
    explicit RecordingClip(QObject *parent = nullptr);
    explicit RecordingClip(const QString &clipPath, QObject *parent = nullptr);

    // Property getters
    QString appId() const { return m_appId; }
    QString clipPath() const { return m_clipPath; }
    QString thumbnailPath() const { return m_thumbnailPath; }
    QDateTime recordingDate() const { return m_recordingDate; }
    QStringList segments() const { return m_segments; }
    int segmentCount() const { return m_segments.size(); }
    QString gameName() const { return m_gameName; }
    qint64 totalSize() const { return m_totalSize; }
    int duration() const { return m_duration; }
    bool isValid() const { return !m_clipPath.isEmpty() && !m_appId.isEmpty(); }
    
    // Formatted getters
    QString formattedDate() const;
    QString formattedSize() const;
    QString formattedDuration() const;

    // Property setters
    void setAppId(const QString &appId);
    void setClipPath(const QString &clipPath);

    // Utility methods
    Q_INVOKABLE bool loadFromPath(const QString &clipPath);
    Q_INVOKABLE QString getSegmentMpdPath(int segmentIndex) const;
    Q_INVOKABLE QUrl getSegmentMpdUrl(int segmentIndex) const;
    Q_INVOKABLE QUrl getThumbnailUrl() const;
    Q_INVOKABLE void refreshSegments();
    Q_INVOKABLE void refreshGameInfo(const QString &steamPath);
    Q_INVOKABLE bool hasMultipleSegments() const { return segmentCount() > 1; }
    
    // Static factory methods
    static RecordingClip* fromClipPath(const QString &clipPath, QObject *parent = nullptr);
    static QList<RecordingClip*> findAllClips(const QString &gameRecordingsPath, QObject *parent = nullptr);

signals:
    void appIdChanged();
    void clipPathChanged();
    void thumbnailPathChanged();
    void recordingDateChanged();
    void segmentsChanged();
    void segmentCountChanged();
    void gameNameChanged();
    void totalSizeChanged();
    void durationChanged();
    void isValidChanged();
    void formattedDateChanged();
    void formattedSizeChanged();
    void formattedDurationChanged();

private:
    // Helper methods
    void parseClipPath();
    void findThumbnail();
    void scanSegments();
    void calculateTotalSize();
    void estimateDuration();
    QDateTime parseRecordingDateTime(const QString &dateStr, const QString &timeStr) const;

    QString formatDuration(int seconds) const;
    
    // Member variables
    QString m_appId;
    QString m_clipPath;
    QString m_thumbnailPath;
    QDateTime m_recordingDate;
    QStringList m_segments;
    QString m_gameName;
    qint64 m_totalSize;
    int m_duration; // in seconds
    
    // Cached values
    mutable QString m_cachedFormattedDate;
    mutable QString m_cachedFormattedSize;
    mutable QString m_cachedFormattedDuration;
    mutable bool m_formattedDateCached;
    mutable bool m_formattedSizeCached;
    mutable bool m_formattedDurationCached;
};

#endif // RECORDINGCLIP_H