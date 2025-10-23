#ifndef QVDFPARSER_H
#define QVDFPARSER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QIODevice>
#include <QTextStream>
#include <memory>

/**
 * @brief Qt-style VDF (Valve Data Format) parser
 * 
 * This class provides a Qt-friendly interface for parsing VDF files,
 * which are used by Steam for configuration and data storage.
 */
class QVdfParser : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Parsing options for VDF files
     */
    struct Options {
        bool stripEscapeSymbols = true;           ///< Remove escape symbols like `\"` and `\\`
        bool ignoreAllPlatformConditionals = false; ///< Ignore platform-specific sections
        bool ignoreIncludes = false;              ///< Ignore #include and #base directives
        
        Options() = default;
    };

    /**
     * @brief Writing options for VDF files
     */
    struct WriteOptions {
        bool escapeSymbols = true;                ///< Add escape symbols when writing
        
        WriteOptions() = default;
    };

    /**
     * @brief VDF object node representing a section or key-value pair
     */
    class VdfObject
    {
    public:
        VdfObject() = default;
        explicit VdfObject(const QString &name);
        
        // Name of this object/section
        QString name() const { return m_name; }
        void setName(const QString &name) { m_name = name; }
        
        // Attributes (key-value pairs)
        QVariantMap attributes() const { return m_attributes; }
        void addAttribute(const QString &key, const QVariant &value);
        QVariant attribute(const QString &key, const QVariant &defaultValue = QVariant()) const;
        bool hasAttribute(const QString &key) const;
        void removeAttribute(const QString &key);
        
        // Child objects (nested sections)
        QList<VdfObject*> children() const { return m_children; }
        void addChild(VdfObject *child);
        VdfObject* child(const QString &name) const;
        QList<VdfObject*> children(const QString &name) const;
        bool hasChild(const QString &name) const;
        void removeChild(VdfObject *child);
        void removeChild(const QString &name);
        
        // Path-based access methods
        VdfObject* child(const QStringList &path) const;
        VdfObject* childByPath(const QString &path, const QString &separator = "/") const;
        bool hasChildByPath(const QString &path, const QString &separator = "/") const;
        
        // Convenience methods
        QString stringAttribute(const QString &key, const QString &defaultValue = QString()) const;
        int intAttribute(const QString &key, int defaultValue = 0) const;
        qint64 longAttribute(const QString &key, qint64 defaultValue = 0) const;
        bool boolAttribute(const QString &key, bool defaultValue = false) const;
        
        // Utility
        bool isEmpty() const;
        void clear();
        
        // Debug
        QString toString(int indent = 0) const;
        
    private:
        QString m_name;
        QVariantMap m_attributes;
        QList<VdfObject*> m_children;
    };

    explicit QVdfParser(QObject *parent = nullptr);
    ~QVdfParser();

    // Parsing methods
    std::unique_ptr<VdfObject> parse(const QString &content) const;
    std::unique_ptr<VdfObject> parse(const QString &content, const Options &options) const;
    std::unique_ptr<VdfObject> parseFile(const QString &filePath) const;
    std::unique_ptr<VdfObject> parseFile(const QString &filePath, const Options &options) const;
    std::unique_ptr<VdfObject> parseDevice(QIODevice *device) const;
    std::unique_ptr<VdfObject> parseDevice(QIODevice *device, const Options &options) const;

    // Writing methods
    QString write(const VdfObject *object) const;
    QString write(const VdfObject *object, const WriteOptions &options) const;
    bool writeToFile(const VdfObject *object, const QString &filePath) const;
    bool writeToFile(const VdfObject *object, const QString &filePath, const WriteOptions &options) const;
    bool writeToDevice(const VdfObject *object, QIODevice *device) const;
    bool writeToDevice(const VdfObject *object, QIODevice *device, const WriteOptions &options) const;
    
    // Convenience methods for common VDF parsing tasks
    static QString findValue(const QString &content, const QString &key);
    static QVariantMap parseSimpleKeyValues(const QString &content);
    
    // Error handling
    QString lastError() const { return m_lastError; }
    bool hasError() const { return !m_lastError.isEmpty(); }
    void clearError() { m_lastError.clear(); }

signals:
    void parseError(const QString &error);

private:
    // Internal parsing methods
    VdfObject *parseInternal(const QString &content, const Options &options);
    bool parseObjectContent(VdfObject *object, const QString &content, int &pos, const Options &options) const;
    QString writeInternal(const VdfObject *object, const WriteOptions &options, int indentLevel = 0) const;
    
    // Helper methods
    QString escapeString(const QString &str) const;
    QString unescapeString(const QString &str) const;
    bool isWhitespace(QChar c) const;
    QString skipWhitespace(const QString &content, int &pos) const;
    QString readQuotedString(const QString &content, int &pos) const;
    QString readUnquotedString(const QString &content, int &pos) const;
    void skipComments(const QString &content, int &pos) const;
    
    mutable QString m_lastError;
};

Q_DECLARE_METATYPE(QVdfParser::VdfObject*)

#endif // QVDFPARSER_H
