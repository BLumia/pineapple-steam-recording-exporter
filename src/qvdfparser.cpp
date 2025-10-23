#include "qvdfparser.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

// VdfObject implementation
QVdfParser::VdfObject::VdfObject(const QString &name)
    : m_name(name)
{
}

void QVdfParser::VdfObject::addAttribute(const QString &key, const QVariant &value)
{
    m_attributes[key] = value;
}

QVariant QVdfParser::VdfObject::attribute(const QString &key, const QVariant &defaultValue) const
{
    return m_attributes.value(key, defaultValue);
}

bool QVdfParser::VdfObject::hasAttribute(const QString &key) const
{
    return m_attributes.contains(key);
}

void QVdfParser::VdfObject::removeAttribute(const QString &key)
{
    m_attributes.remove(key);
}

void QVdfParser::VdfObject::addChild(VdfObject *child)
{
    if (child && !m_children.contains(child)) {
        m_children.append(child);
    }
}

QVdfParser::VdfObject* QVdfParser::VdfObject::child(const QString &name) const
{
    for (VdfObject *child : m_children) {
        if (child && child->name() == name) {
            return child;
        }
    }
    return nullptr;
}

QList<QVdfParser::VdfObject*> QVdfParser::VdfObject::children(const QString &name) const
{
    QList<VdfObject*> result;
    for (VdfObject *child : m_children) {
        if (child && child->name() == name) {
            result.append(child);
        }
    }
    return result;
}

bool QVdfParser::VdfObject::hasChild(const QString &name) const
{
    return child(name) != nullptr;
}

void QVdfParser::VdfObject::removeChild(VdfObject *child)
{
    m_children.removeAll(child);
}

void QVdfParser::VdfObject::removeChild(const QString &name)
{
    for (int i = m_children.size() - 1; i >= 0; --i) {
        if (m_children[i] && m_children[i]->name() == name) {
            m_children.removeAt(i);
        }
    }
}

QVdfParser::VdfObject* QVdfParser::VdfObject::child(const QStringList &path) const
{
    if (path.isEmpty()) {
        return nullptr;
    }
    
    VdfObject *current = const_cast<VdfObject*>(this);
    for (const QString &segment : path) {
        if (!current) {
            return nullptr;
        }
        current = current->child(segment);
    }
    return current;
}

QVdfParser::VdfObject* QVdfParser::VdfObject::childByPath(const QString &path, const QString &separator) const
{
    if (path.isEmpty()) {
        return nullptr;
    }
    
    QStringList segments = path.split(separator, Qt::SkipEmptyParts);
    return child(segments);
}

bool QVdfParser::VdfObject::hasChildByPath(const QString &path, const QString &separator) const
{
    return childByPath(path, separator) != nullptr;
}

QString QVdfParser::VdfObject::stringAttribute(const QString &key, const QString &defaultValue) const
{
    return attribute(key, defaultValue).toString();
}

int QVdfParser::VdfObject::intAttribute(const QString &key, int defaultValue) const
{
    return attribute(key, defaultValue).toInt();
}

qint64 QVdfParser::VdfObject::longAttribute(const QString &key, qint64 defaultValue) const
{
    return attribute(key, defaultValue).toLongLong();
}

bool QVdfParser::VdfObject::boolAttribute(const QString &key, bool defaultValue) const
{
    QVariant value = attribute(key);
    if (value.isNull()) {
        return defaultValue;
    }
    
    // Handle string representations of boolean values
    if (value.type() == QVariant::String) {
        QString str = value.toString().toLower();
        return str == "1" || str == "true" || str == "yes";
    }
    
    return value.toBool();
}

bool QVdfParser::VdfObject::isEmpty() const
{
    return m_attributes.isEmpty() && m_children.isEmpty();
}

void QVdfParser::VdfObject::clear()
{
    m_attributes.clear();
    qDeleteAll(m_children);
    m_children.clear();
}

QString QVdfParser::VdfObject::toString(int indent) const
{
    QString result;
    QString indentStr = QString("\t").repeated(indent);
    
    result += QString("%1\"%2\"\n%1{\n").arg(indentStr, m_name);
    
    // Add attributes
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it) {
        result += QString("%1\t\"%2\"\t\t\"%3\"\n").arg(indentStr, it.key(), it.value().toString());
    }
    
    // Add children
    for (VdfObject *child : m_children) {
        if (child) {
            result += child->toString(indent + 1);
        }
    }
    
    result += QString("%1}\n").arg(indentStr);
    return result;
}

// QVdfParser implementation
QVdfParser::QVdfParser(QObject *parent)
    : QObject(parent)
{
}

QVdfParser::~QVdfParser()
{
}

std::unique_ptr<QVdfParser::VdfObject> QVdfParser::parse(const QString &content) const
{
    return parse(content, Options());
}

std::unique_ptr<QVdfParser::VdfObject> QVdfParser::parse(const QString &content, const Options &options) const
{
    VdfObject *result = const_cast<QVdfParser*>(this)->parseInternal(content, options);
    return std::unique_ptr<VdfObject>(result);
}

std::unique_ptr<QVdfParser::VdfObject> QVdfParser::parseFile(const QString &filePath) const
{
    return parseFile(filePath, Options());
}

std::unique_ptr<QVdfParser::VdfObject> QVdfParser::parseFile(const QString &filePath, const Options &options) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const_cast<QVdfParser*>(this)->m_lastError = QString("Cannot open file: %1").arg(filePath);
        return nullptr;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    return parse(content, options);
}

std::unique_ptr<QVdfParser::VdfObject> QVdfParser::parseDevice(QIODevice *device) const
{
    return parseDevice(device, Options());
}

std::unique_ptr<QVdfParser::VdfObject> QVdfParser::parseDevice(QIODevice *device, const Options &options) const
{
    if (!device || !device->isReadable()) {
        const_cast<QVdfParser*>(this)->m_lastError = "Device is not readable";
        return nullptr;
    }
    
    QTextStream stream(device);
    QString content = stream.readAll();
    return parse(content, options);
}

QString QVdfParser::write(const VdfObject *object) const
{
    return write(object, WriteOptions());
}

QString QVdfParser::write(const VdfObject *object, const WriteOptions &options) const
{
    if (!object) {
        return QString();
    }
    
    return writeInternal(object, options);
}

bool QVdfParser::writeToFile(const VdfObject *object, const QString &filePath) const
{
    return writeToFile(object, filePath, WriteOptions());
}

bool QVdfParser::writeToFile(const VdfObject *object, const QString &filePath, const WriteOptions &options) const
{
    if (!object) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    return writeToDevice(object, &file, options);
}

bool QVdfParser::writeToDevice(const VdfObject *object, QIODevice *device) const
{
    return writeToDevice(object, device, WriteOptions());
}

bool QVdfParser::writeToDevice(const VdfObject *object, QIODevice *device, const WriteOptions &options) const
{
    if (!object || !device || !device->isWritable()) {
        return false;
    }
    
    QString content = writeInternal(object, options);
    QTextStream stream(device);
    stream << content;
    return true;
}

QString QVdfParser::findValue(const QString &content, const QString &key)
{
    // Simple regex-based approach for quick value lookup
    QRegularExpression regex(QString("\"%1\"\\s*\"([^\"]*)\"").arg(QRegularExpression::escape(key)));
    QRegularExpressionMatch match = regex.match(content);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return QString();
}

QVariantMap QVdfParser::parseSimpleKeyValues(const QString &content)
{
    QVariantMap result;
    
    // Match key-value pairs: "key" "value"
    QRegularExpression regex("\"([^\"]+)\"\\s*\"([^\"]*)\"");
    QRegularExpressionMatchIterator it = regex.globalMatch(content);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString key = match.captured(1);
        QString value = match.captured(2);
        result[key] = value;
    }
    
    return result;
}

QVdfParser::VdfObject *QVdfParser::parseInternal(const QString &content, const Options &options)
{
    int pos = 0;
    const int length = content.length();
    
    // Skip initial whitespace and comments
    skipWhitespace(content, pos);
    skipComments(content, pos);
    skipWhitespace(content, pos);
    
    if (pos >= length) {
        m_lastError = "Empty or invalid VDF content";
        emit parseError(m_lastError);
        return nullptr;
    }
    
    try {
        // Parse the root object
        VdfObject *root = new VdfObject();
        
        // Read the root object name
        if (content[pos] == '"') {
            root->setName(readQuotedString(content, pos));
        } else {
            root->setName(readUnquotedString(content, pos));
        }
        
        skipWhitespace(content, pos);
        skipComments(content, pos);
        skipWhitespace(content, pos);
        
        if (pos >= length || content[pos] != '{') {
            delete root;
            m_lastError = "Expected '{' after object name";
            emit parseError(m_lastError);
            return nullptr;
        }
        
        pos++; // Skip '{'
        
        // Parse object content
        if (!parseObjectContent(root, content, pos, options)) {
            delete root;
            return nullptr;
        }
        
        return root;
        
    } catch (const QString &error) {
        m_lastError = error;
        emit parseError(m_lastError);
        return nullptr;
    }
}

bool QVdfParser::parseObjectContent(VdfObject *object, const QString &content, int &pos, const Options &options) const
{
    const int length = content.length();
    
    while (pos < length) {
        skipWhitespace(content, pos);
        skipComments(content, pos);
        skipWhitespace(content, pos);
        
        if (pos >= length) {
            throw QString("Unexpected end of content");
        }
        
        if (content[pos] == '}') {
            pos++; // Skip '}'
            return true;
        }
        
        // Read key
        QString key;
        if (content[pos] == '"') {
            key = readQuotedString(content, pos);
        } else {
            key = readUnquotedString(content, pos);
        }
        
        if (key.isEmpty()) {
            throw QString("Empty key found");
        }
        
        skipWhitespace(content, pos);
        skipComments(content, pos);
        skipWhitespace(content, pos);
        
        if (pos >= length) {
            throw QString("Unexpected end after key");
        }
        
        // Check if this is a nested object or a key-value pair
        if (content[pos] == '{') {
            // Nested object
            pos++; // Skip '{'
            
            VdfObject *child = new VdfObject(key);
            if (!parseObjectContent(child, content, pos, options)) {
                delete child;
                return false;
            }
            
            object->addChild(child);
            
        } else {
            // Key-value pair
            QString value;
            if (content[pos] == '"') {
                value = readQuotedString(content, pos);
            } else {
                value = readUnquotedString(content, pos);
            }
            
            if (options.stripEscapeSymbols) {
                value = unescapeString(value);
            }
            
            object->addAttribute(key, value);
        }
    }
    
    throw QString("Missing closing brace");
}

QString QVdfParser::writeInternal(const VdfObject *object, const WriteOptions &options, int indentLevel) const
{
    QString result;
    QString indent = QString("\t").repeated(indentLevel);
    
    QString objectName = object->name();
    if (options.escapeSymbols) {
        objectName = escapeString(objectName);
    }
    
    result += QString("%1\"%2\"\n%1{\n").arg(indent, objectName);
    
    // Write attributes
    QVariantMap attributes = object->attributes();
    for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
        QString key = it.key();
        QString value = it.value().toString();
        
        if (options.escapeSymbols) {
            key = escapeString(key);
            value = escapeString(value);
        }
        
        result += QString("%1\t\"%2\"\t\t\"%3\"\n").arg(indent, key, value);
    }
    
    // Write children
    QList<VdfObject*> children = object->children();
    for (VdfObject *child : children) {
        if (child) {
            result += writeInternal(child, options, indentLevel + 1);
        }
    }
    
    result += QString("%1}\n").arg(indent);
    return result;
}

QString QVdfParser::escapeString(const QString &str) const
{
    QString result = str;
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    return result;
}

QString QVdfParser::unescapeString(const QString &str) const
{
    QString result = str;
    result.replace("\\\"", "\"");
    result.replace("\\\\", "\\");
    return result;
}

bool QVdfParser::isWhitespace(QChar c) const
{
    return c.isSpace();
}

QString QVdfParser::skipWhitespace(const QString &content, int &pos) const
{
    const int length = content.length();
    while (pos < length && isWhitespace(content[pos])) {
        pos++;
    }
    return QString();
}

QString QVdfParser::readQuotedString(const QString &content, int &pos) const
{
    const int length = content.length();
    
    if (pos >= length || content[pos] != '"') {
        throw QString("Expected quoted string");
    }
    
    pos++; // Skip opening quote
    
    QString result;
    bool escaped = false;
    
    while (pos < length) {
        QChar c = content[pos];
        
        if (escaped) {
            result += c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            pos++; // Skip closing quote
            return result;
        } else {
            result += c;
        }
        
        pos++;
    }
    
    throw QString("Unterminated quoted string");
}

QString QVdfParser::readUnquotedString(const QString &content, int &pos) const
{
    const int length = content.length();
    QString result;
    
    while (pos < length) {
        QChar c = content[pos];
        
        if (isWhitespace(c) || c == '{' || c == '}' || c == '"') {
            break;
        }
        
        result += c;
        pos++;
    }
    
    return result;
}

void QVdfParser::skipComments(const QString &content, int &pos) const
{
    const int length = content.length();
    
    while (pos < length && content[pos] == '/') {
        if (pos + 1 < length) {
            if (content[pos + 1] == '/') {
                // Line comment
                pos += 2;
                while (pos < length && content[pos] != '\n') {
                    pos++;
                }
                if (pos < length) pos++; // Skip newline
            } else if (content[pos + 1] == '*') {
                // Block comment
                pos += 2;
                while (pos + 1 < length) {
                    if (content[pos] == '*' && content[pos + 1] == '/') {
                        pos += 2;
                        break;
                    }
                    pos++;
                }
            } else {
                break;
            }
        } else {
            break;
        }
        
        skipWhitespace(content, pos);
    }
}