#pragma once

#include <QString>
#include <QMap>

namespace wipepdf {

enum class Language {
    Zh,
    En
};

class I18n {
public:
    static I18n &instance();

    Language currentLanguage() const { return m_lang; }
    void setLanguage(Language lang) { m_lang = lang; }

    QString text(const QString &key) const;

private:
    I18n();
    Language m_lang = Language::Zh;
    QMap<QString, QString> m_zh;
    QMap<QString, QString> m_en;
};

inline QString tr_(const QString &key) {
    return I18n::instance().text(key);
}

} // namespace wipepdf
