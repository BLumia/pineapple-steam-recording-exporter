#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <QObject>
#include <QString>
#include <QDateTime>

class GameInfo : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString appId READ appId WRITE setAppId NOTIFY appIdChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString installDir READ installDir WRITE setInstallDir NOTIFY installDirChanged)
    Q_PROPERTY(QString developer READ developer WRITE setDeveloper NOTIFY developerChanged)
    Q_PROPERTY(QString publisher READ publisher WRITE setPublisher NOTIFY publisherChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated WRITE setLastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(qint64 sizeOnDisk READ sizeOnDisk WRITE setSizeOnDisk NOTIFY sizeOnDiskChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)

public:
    explicit GameInfo(QObject *parent = nullptr);
    explicit GameInfo(const QString &appId, QObject *parent = nullptr);

    // Property getters
    QString appId() const { return m_appId; }
    QString name() const { return m_name; }
    QString installDir() const { return m_installDir; }
    QString developer() const { return m_developer; }
    QString publisher() const { return m_publisher; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    qint64 sizeOnDisk() const { return m_sizeOnDisk; }
    bool isValid() const { return !m_appId.isEmpty() && !m_name.isEmpty(); }

    // Property setters
    void setAppId(const QString &appId);
    void setName(const QString &name);
    void setInstallDir(const QString &installDir);
    void setDeveloper(const QString &developer);
    void setPublisher(const QString &publisher);
    void setLastUpdated(const QDateTime &lastUpdated);
    void setSizeOnDisk(qint64 sizeOnDisk);

    // Static methods for loading game info
    static GameInfo* fromAcfFile(const QString &acfFilePath, QObject *parent = nullptr);
    static GameInfo* fromAppId(const QString &appId, const QString &steamPath, QObject *parent = nullptr);

    // Utility methods
    Q_INVOKABLE QString formattedSize() const;
    Q_INVOKABLE QString formattedLastUpdated() const;
    Q_INVOKABLE bool loadFromAcf(const QString &acfFilePath);
    Q_INVOKABLE void clear();

    // Comparison operators
    bool operator==(const GameInfo &other) const;
    bool operator!=(const GameInfo &other) const;

signals:
    void appIdChanged();
    void nameChanged();
    void installDirChanged();
    void developerChanged();
    void publisherChanged();
    void lastUpdatedChanged();
    void sizeOnDiskChanged();
    void isValidChanged();

private:
    // Helper methods for parsing ACF files
    QString parseAcfValue(const QString &content, const QString &key) const;
    QDateTime parseAcfDateTime(const QString &timestampString) const;
    qint64 parseAcfSize(const QString &sizeString) const;
    QString findAcfFilePath(const QString &appId, const QString &steamPath) const;

    // Member variables
    QString m_appId;
    QString m_name;
    QString m_installDir;
    QString m_developer;
    QString m_publisher;
    QDateTime m_lastUpdated;
    qint64 m_sizeOnDisk;
};

#endif // GAMEINFO_H