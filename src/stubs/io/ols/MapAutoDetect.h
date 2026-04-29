#pragma once
#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVector>
#include <cstdint>
class Project;
namespace ols {
struct MapCandidate {
    QString  name;
    quint32  romAddress = 0;
    quint32  width = 1;
    quint32  height = 1;
    quint8   cellBytes = 2;
    bool     cellSigned = false;
    bool     bigEndian = false;
    double   score = 0;
    QString  reason;
};
struct MapAutoDetectOptions {
    bool tryBigEndianAxes = false;
    int  minScore2D = 60, minScore1D = 65;
    int  maxCandidatesPerRegion = 20000;
    int  maxAxesPerRegion = 2048;
};
class MapAutoDetect {
    Q_DECLARE_TR_FUNCTIONS(ols::MapAutoDetect)
public:
    static QVector<MapCandidate> scan(const QByteArray &, quint32, const MapAutoDetectOptions & = {}) { return {}; }
    static QVector<MapCandidate> scanProject(const Project &, const MapAutoDetectOptions & = {}) { return {}; }
};
}
