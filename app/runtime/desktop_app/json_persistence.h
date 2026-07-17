#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace desktop_app {

bool writeJsonDocumentAtomically(const QString& path, const QJsonDocument& document, QString* error = nullptr,
                                 QJsonDocument::JsonFormat format = QJsonDocument::Indented);

bool writeJsonObjectAtomically(const QString& path, const QJsonObject& object, QString* error = nullptr,
                               QJsonDocument::JsonFormat format = QJsonDocument::Indented);

} // namespace desktop_app
