#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QObject>
#include <QTranslator>
#include <QLocale>
#include <QStringList>

class LanguageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)

public:
    explicit LanguageManager(QObject *parent = nullptr);
    ~LanguageManager();

    QStringList availableLanguages() const;
    QString currentLanguage() const;

    Q_INVOKABLE void setLanguage(const QString &languageCode);

signals:
    void currentLanguageChanged();

private:
    bool loadLanguage(const QString &languageCode);
    
    QTranslator *m_translator;
    QString m_currentLanguageCode;
    QStringList m_availableLanguages;
};

#endif // LANGUAGEMANAGER_H
