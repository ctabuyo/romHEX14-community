/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>
#include "romdata.h"

struct KPVehicleInfo {
    QString manufacturer, model, variant, year, power;
    QString ecuBrand, partNumber, swVersion;
    uint32_t romWordCount = 0;  // from header hex string e.g. 0x20000
    uint32_t romByteSize = 0;   // romWordCount * 2
};

class KPParser : public QObject {
    Q_OBJECT
public:
    explicit KPParser(QObject *parent = nullptr);
    bool parse(const QByteArray &fileData);

    KPVehicleInfo vehicleInfo() const { return m_info; }
    QVector<MapInfo> maps() const { return m_maps; }
    QString errorString() const { return m_error; }

signals:
    void progress(const QString &stage, int pct);

private:
    bool parseHeader(const QByteArray &data);
    QByteArray extractIntern(const QByteArray &data);
    void parseMapRecords(const QByteArray &internData);
    void parseMapRecordsV2(const QByteArray &internData);  // length-prefixed format

    KPVehicleInfo m_info;
    QVector<MapInfo> m_maps;
    QString m_error;
};
