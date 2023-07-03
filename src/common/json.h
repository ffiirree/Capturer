#ifndef CAPTURER_JSON_H
#define CAPTURER_JSON_H

#include <QKeySequence>
#include <QColor>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

inline void from_json(const json& j, QString& qstr)         { qstr = QString::fromStdString(j.get<std::string>()); }
inline void to_json(json& j, const QString& qstr)           { j = qstr.toStdString(); }

inline void from_json(const json& j, QColor& c)             { c = { j.get<QString>() }; }
inline void to_json(json& j, const QColor& c)               { j = c.name(QColor::HexArgb); }

inline void from_json(const json& j, QKeySequence& key)     { key = { j.get<QString>() }; }
inline void to_json(json& j, const QKeySequence& key)       { j = key.toString(); }

inline void from_json(const json& j, Qt::PenStyle& style)   { style = Qt::PenStyle(j.get<int>()); }
inline void to_json(json& j, const Qt::PenStyle& style)     { j = style; }

#endif //! CAPTURER_JSON_H
