/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>
#include "romdata.h"

// Per-axis descriptor extracted from AXIS_DESCR blocks
struct A2LAxisDesc {
    QString axisType;           // "STD_AXIS", "COM_AXIS", "FIX_AXIS", "CURVE_AXIS"
    QString compuMethodRef;     // compu method for axis tick labels
    QString inputName;          // input quantity name
    QString axisPtsRef;         // COM_AXIS: referenced AXIS_PTS name
    QVector<double> fixedValues;// FIX_AXIS_PAR / FIX_AXIS_PAR_DIST values
    int     maxCount = 0;
};

struct A2LCharacteristic {
    QString name;
    QString description;
    QString type;
    uint32_t rawAddress = 0;
    MapDimensions dimensions;
    int dataSize = 2;
    bool dataSigned = false;
    int dataLength = 0;
    QString compuMethodRef;
    QString recordLayoutRef;    // name of the RECORD_LAYOUT used
    A2LAxisDesc xAxisDesc;      // first  AXIS_DESCR
    A2LAxisDesc yAxisDesc;      // second AXIS_DESCR
};

// AXIS_PTS object — a shared axis array stored in ROM
struct A2LAxisPts {
    QString  name;
    uint32_t rawAddress = 0;
    uint32_t address    = 0;    // file offset
    int      maxCount   = 0;
    int      dataSize   = 2;
    QString  compuMethodRef;
    QString  recordLayoutRef;
};

// A2L GROUP block — organises characteristics into named folders
struct A2LGroup {
    QString     name;
    QString     description;
    QStringList characteristics; // REF_CHARACTERISTIC names
    QStringList subGroups;       // SUB_GROUP names
};

class A2LParser : public QObject {
    Q_OBJECT

public:
    explicit A2LParser(QObject *parent = nullptr);

    void parse(const QString &text, int romSize = 0, uint32_t knownBase = 0);
    uint32_t detectBaseAddress(int romSize);
    QVector<MapInfo> getMapList() const;

    ByteOrder byteOrder()    const { return m_byteOrder; }
    uint32_t  baseAddress()  const { return m_baseAddress; }
    const QVector<A2LCharacteristic>& characteristics() const { return m_characteristics; }
    const QVector<A2LGroup>&          groups()          const { return m_groups; }

signals:
    void progress(const QString &stage, int pct);

private:
    void parseCharacteristics(const QString &text);
    void parseAxisPts(const QString &text);
    void parseCompuMethods(const QString &text);
    void parseGroups(const QString &text);
    void parseRecordLayouts(const QString &text);
    void generateAutoGroups();
    int  estimateLength(const A2LCharacteristic &ch) const;

    QVector<A2LCharacteristic>  m_characteristics;
    QVector<A2LGroup>           m_groups;
    QMap<QString, CompuMethod>  m_compuMethods;
    QMap<QString, A2LAxisPts>   m_axisPts;
    QMap<QString, bool>         m_recordLayoutColMajor; // layout name → true if COLUMN_DIR
    QMap<QString, int>          m_recordLayoutDataSize; // layout name → FNC_VALUES byte size
    QMap<QString, bool>         m_recordLayoutSigned;   // layout name → FNC_VALUES is signed
    QMap<QString, int>          m_recordLayoutAxisSkip;  // layout name → NO_AXIS_PTS_X byte size
    QMap<QString, int>          m_recordLayoutAxisSkipY; // layout name → NO_AXIS_PTS_Y byte size
    QMap<QString, int>          m_recordLayoutAxisSize;  // layout name → AXIS_PTS_X datatype byte size
    QMap<QString, int>          m_recordLayoutAxisSizeY; // layout name → AXIS_PTS_Y datatype byte size
    ByteOrder m_byteOrder  = ByteOrder::BigEndian;
    uint32_t  m_baseAddress = 0;
};
