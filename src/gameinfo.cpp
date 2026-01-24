#include "gameinfo.h"
#include "qvdfparser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QLocale>

GameInfo::GameInfo(QObject *parent)
    : QObject(parent)
    , m_sizeOnDisk(0)
{
}

GameInfo::GameInfo(const QString &appId, QObject *parent)
    : QObject(parent)
    , m_appId(appId)
    , m_sizeOnDisk(0)
{
}

void GameInfo::setAppId(const QString &appId)
{
    if (m_appId != appId) {
        m_appId = appId;
        emit appIdChanged();
        emit isValidChanged();
    }
}

void GameInfo::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
        emit isValidChanged();
    }
}

void GameInfo::setInstallDir(const QString &installDir)
{
    if (m_installDir != installDir) {
        m_installDir = installDir;
        emit installDirChanged();
    }
}

void GameInfo::setDeveloper(const QString &developer)
{
    if (m_developer != developer) {
        m_developer = developer;
        emit developerChanged();
    }
}

void GameInfo::setPublisher(const QString &publisher)
{
    if (m_publisher != publisher) {
        m_publisher = publisher;
        emit publisherChanged();
    }
}

void GameInfo::setLastUpdated(const QDateTime &lastUpdated)
{
    if (m_lastUpdated != lastUpdated) {
        m_lastUpdated = lastUpdated;
        emit lastUpdatedChanged();
    }
}

void GameInfo::setSizeOnDisk(qint64 sizeOnDisk)
{
    if (m_sizeOnDisk != sizeOnDisk) {
        m_sizeOnDisk = sizeOnDisk;
        emit sizeOnDiskChanged();
    }
}

GameInfo* GameInfo::fromAcfFile(const QString &acfFilePath, QObject *parent)
{
    GameInfo *gameInfo = new GameInfo(parent);
    
    if (gameInfo->loadFromAcf(acfFilePath)) {
        return gameInfo;
    } else {
        delete gameInfo;
        return nullptr;
    }
}

GameInfo* GameInfo::fromAppId(const QString &appId, const QString &steamPath, QObject *parent)
{
    GameInfo *gameInfo = new GameInfo(appId, parent);
    
    QString acfFilePath = gameInfo->findAcfFilePath(appId, steamPath);
    if (!acfFilePath.isEmpty() && gameInfo->loadFromAcf(acfFilePath)) {
        return gameInfo;
    } else {
        delete gameInfo;
        return nullptr;
    }
}

QString GameInfo::formattedSize() const
{
    if (m_sizeOnDisk <= 0) {
        return tr("Unknown");
    }

    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;

    if (m_sizeOnDisk >= gb) {
        return tr("%1 GB").arg(QString::number(static_cast<double>(m_sizeOnDisk) / gb, 'f', 2));
    } else if (m_sizeOnDisk >= mb) {
        return tr("%1 MB").arg(QString::number(static_cast<double>(m_sizeOnDisk) / mb, 'f', 1));
    } else if (m_sizeOnDisk >= kb) {
        return tr("%1 KB").arg(QString::number(static_cast<double>(m_sizeOnDisk) / kb, 'f', 1));
    } else {
        return tr("%1 bytes").arg(m_sizeOnDisk);
    }
}

QString GameInfo::formattedLastUpdated() const
{
    if (!m_lastUpdated.isValid()) {
        return tr("Unknown");
    }

    QLocale locale;
    return locale.toString(m_lastUpdated, QLocale::ShortFormat);
}

bool GameInfo::loadFromAcf(const QString &acfFilePath)
{
    QVdfParser parser;
    auto root = parser.parseFile(acfFilePath);
    
    if (!root) {
        qWarning() << "Failed to parse ACF file:" << acfFilePath;
        qWarning() << "Parser error:" << parser.lastError();
        return false;
    }

    // Parse the ACF content using VDF parser
    QString appId = root->stringAttribute("appid");
    QString name = root->stringAttribute("name");
    QString installDir = root->stringAttribute("installdir");
    QString developer = root->stringAttribute("developer");
    QString publisher = root->stringAttribute("publisher");
    QString lastUpdatedStr = root->stringAttribute("LastUpdated");
    QString sizeOnDiskStr = root->stringAttribute("SizeOnDisk");

    // Validate required fields
    if (appId.isEmpty() || name.isEmpty()) {
        qWarning() << "ACF file missing required fields (appid or name):" << acfFilePath;
        return false;
    }

    // Set the parsed values
    setAppId(appId);
    setName(name);
    setInstallDir(installDir);
    setDeveloper(developer);
    setPublisher(publisher);
    setLastUpdated(parseAcfDateTime(lastUpdatedStr));
    setSizeOnDisk(parseAcfSize(sizeOnDiskStr));

    qDebug() << "Successfully loaded game info for:" << name << "(AppID:" << appId << ")";
    return true;
}

void GameInfo::clear()
{
    setAppId(QString());
    setName(QString());
    setInstallDir(QString());
    setDeveloper(QString());
    setPublisher(QString());
    setLastUpdated(QDateTime());
    setSizeOnDisk(0);
}

bool GameInfo::operator==(const GameInfo &other) const
{
    return m_appId == other.m_appId;
}

bool GameInfo::operator!=(const GameInfo &other) const
{
    return !(*this == other);
}

QString GameInfo::parseAcfValue(const QString &content, const QString &key) const
{
    // ACF format uses quoted keys and values
    // Example: "name"		"Game Name"
    QRegularExpression regex(QString("\"%1\"\\s+\"([^\"]+)\"").arg(QRegularExpression::escape(key)));
    QRegularExpressionMatch match = regex.match(content);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return QString();
}

QDateTime GameInfo::parseAcfDateTime(const QString &timestampString) const
{
    if (timestampString.isEmpty()) {
        return QDateTime();
    }

    bool ok;
    qint64 timestamp = timestampString.toLongLong(&ok);
    
    if (ok && timestamp > 0) {
        return QDateTime::fromSecsSinceEpoch(timestamp);
    }

    return QDateTime();
}

qint64 GameInfo::parseAcfSize(const QString &sizeString) const
{
    if (sizeString.isEmpty()) {
        return 0;
    }

    bool ok;
    qint64 size = sizeString.toLongLong(&ok);
    
    return ok ? size : 0;
}

QString GameInfo::findAcfFilePath(const QString &appId, const QString &steamPath) const
{
    if (appId.isEmpty() || steamPath.isEmpty()) {
        return QString();
    }

    // Prefer reading libraryfolders.vdf to enumerate all Steam libraries
    const QString libraryFoldersPath = steamPath + "/config/libraryfolders.vdf";
    QVdfParser parser;
    auto libraries = parser.parseFile(libraryFoldersPath);

    const QString acfFileName = QString("appmanifest_%1.acf").arg(appId);

    if (libraries && libraries->name() == QLatin1String("libraryfolders")) {
        const auto libraryEntries = libraries->children();
        for (const auto *entry : libraryEntries) {
            if (!entry) continue;
            const QString libraryPath = entry->stringAttribute("path");
            if (libraryPath.isEmpty()) continue;

            const QString normalizedLibraryPath = QDir::fromNativeSeparators(libraryPath);
            QDir steamAppsDir(normalizedLibraryPath + "/steamapps");
            const QString acfFilePath = steamAppsDir.absoluteFilePath(acfFileName);

            if (QFileInfo::exists(acfFilePath)) {
                return acfFilePath;
            }

            // Optional: if apps list indicates presence, still try path
            if (auto apps = entry->child("apps")) {
                if (apps->hasAttribute(appId)) {
                    if (QFileInfo::exists(acfFilePath)) {
                        return acfFilePath;
                    }
                }
            }
        }
    } else {
        qWarning() << "Failed to parse libraryfolders.vdf:" << libraryFoldersPath << "error:" << parser.lastError();
    }

    // Fallback: check default steamapps under steamPath
    QDir defaultSteamAppsDir(steamPath + "/steamapps");
    if (defaultSteamAppsDir.exists()) {
        const QString fallbackAcfPath = defaultSteamAppsDir.absoluteFilePath(acfFileName);
        if (QFileInfo::exists(fallbackAcfPath)) {
            return fallbackAcfPath;
        }
    } else {
        qWarning() << "Default Steamapps directory does not exist:" << defaultSteamAppsDir.absolutePath();
    }

    qWarning() << "ACF file not found for AppID" << appId << "in any library defined in" << libraryFoldersPath;
    return QString();
}
