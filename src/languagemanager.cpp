#include "languagemanager.h"
#include <QGuiApplication>
#include <QDebug>

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent), m_translator(nullptr)
{
    // Initialize available languages based on translation files
    m_availableLanguages << "en" << "zh_CN" << "de" << "nl" << "pl" << "tr";
    
    // Load initial language based on system locale
    QString systemLanguage = QLocale::system().name();
    if (m_availableLanguages.contains(systemLanguage)) {
        m_currentLanguageCode = systemLanguage;
    } else {
        m_currentLanguageCode = "en"; // Default to English
    }
    
    loadLanguage(m_currentLanguageCode);
}

LanguageManager::~LanguageManager()
{
    if (m_translator) {
        QGuiApplication::removeTranslator(m_translator);
        delete m_translator;
    }
}

QStringList LanguageManager::availableLanguages() const
{
    return m_availableLanguages;
}

QString LanguageManager::currentLanguage() const
{
    return m_currentLanguageCode;
}

void LanguageManager::setLanguage(const QString &languageCode)
{
    if (m_currentLanguageCode == languageCode) {
        return; // Already set to this language
    }
    
    if (m_availableLanguages.contains(languageCode) && loadLanguage(languageCode)) {
        m_currentLanguageCode = languageCode;
        emit currentLanguageChanged();
        qDebug() << "Language changed to:" << languageCode;
    } else {
        qWarning() << "Unsupported language code:" << languageCode;
    }
}

bool LanguageManager::loadLanguage(const QString &languageCode)
{
    // Remove old translator if exists
    if (m_translator) {
        QGuiApplication::removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }
    
    // Create and load new translator
    m_translator = new QTranslator(this);
    
    if (m_translator->load(QLocale(languageCode), "PineappleSteamRecordingExporter", "_", ":/i18n/")) {
        QGuiApplication::installTranslator(m_translator);
        qDebug() << "Translation loaded for language:" << languageCode;
        return true;
    } else {
        qWarning() << "Failed to load translation for language:" << languageCode;
        delete m_translator;
        m_translator = nullptr;
        return false;
    }
}
