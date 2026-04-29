#pragma once
#include <QString>
#include <QByteArray>
namespace ols {
struct EcuDetectionResult {
    QString family, ecuName, ecuVariant, detectorName, detector;
    QString hwNumber, swNumber, swVersion, productionNo, engineCode;
    QString hwAltNumber;
    int confidence = 0; bool ok = false;
    qint64 idBlockOffset = -1;
    QStringList dataAreas;
    QByteArray rawIdBlock;
};
struct EcuMetadataFields {
    QString *producer = nullptr, *ecuName = nullptr;
    QString *hwNumber = nullptr, *swNumber = nullptr;
    QString *swVersion = nullptr, *productionNo = nullptr;
    QString *engineCode = nullptr;
};
class EcuAutoDetect {
public:
    static EcuDetectionResult detect(const QByteArray &) { return {}; }
    static int applyToFields(const EcuDetectionResult &, EcuMetadataFields &, bool = false) { return 0; }
    static QByteArray decodeRom(const QByteArray &data, const QString & = {}) { return data; }
};
}
