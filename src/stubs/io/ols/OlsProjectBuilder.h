#pragma once
#include <QVector>
#include <QString>
class Project;
class QObject;
namespace ols {
struct OlsImportResult;
QVector<Project*> buildProjectsFromOlsImport(const OlsImportResult &, const QString & = {}, QObject * = nullptr);
}
// Stub inline implementation
inline QVector<Project*> ols::buildProjectsFromOlsImport(const OlsImportResult &, const QString &, QObject *) { return {}; }
