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

struct OLSProjectInfo {
    QString brand;
    QString model;
    QString engine;
    QString year;
    QString fuelType;
    QString transmission;
    QString ecuType;
    QString ecuFamily;
    QString partNumber;
    QString swVersion;
    QString description;
    QString fileName;
    QString olsVersion;
};

class OLSParser : public QObject {
    Q_OBJECT
public:
    explicit OLSParser(QObject *parent = nullptr);

    bool parse(const QByteArray &fileData);

    OLSProjectInfo projectInfo() const { return m_info; }
    QByteArray romData() const { return m_romData; }
    uint32_t romSize() const { return m_romSize; }
    QVector<MapInfo> maps() const { return m_maps; }
    ByteOrder byteOrder() const { return m_byteOrder; }
    QString errorString() const { return m_error; }

signals:
    void progress(const QString &stage, int pct);

private:
    QString readString(const QByteArray &data, int &pos);
    void parseHeader(const QByteArray &data);
    void parseMapRecords(const QByteArray &data);
    void extractRomData(const QByteArray &data);

    OLSProjectInfo m_info;
    QByteArray m_romData;
    uint32_t m_romSize = 0;
    QVector<MapInfo> m_maps;
    ByteOrder m_byteOrder = ByteOrder::BigEndian;
    QString m_error;
};
