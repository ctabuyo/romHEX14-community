#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
class Project;
namespace ols {
struct OlsExportResult {
    QByteArray fileData;
    QString error;
    QStringList warnings;
};
class OlsExporter {
public:
    static OlsExportResult exportProject(const Project &) { return {}; }
};
}
