/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class VehicleDatabase {
public:
    static VehicleDatabase &instance();

    struct EcuEntry {
        QString use;          // "Engine", "Gearbox"
        QString manufacturer; // "Bosch", "Siemens", "Continental"
        QString group;        // "EDC15", "MED17"
        QString name;         // "EDC15C11", "MED17.1.6"
    };

    struct ModelEntry {
        QString category;     // "PKW", "Truck", "Motorbike"
        QString brand;        // "Audi", "BMW"
        QString model;        // "A4", "3 Series"
        QString engine;       // "2.0 TDI", "3.0 V6"
    };

    struct GearboxEntry {
        QString brand;
        QString name;
    };

    // ECU queries
    QStringList ecuManufacturers() const;
    QStringList ecuGroups(const QString &manufacturer) const;
    QStringList ecuNames(const QString &group) const;
    QStringList ecuNamesFlat() const;

    // Vehicle queries
    QStringList categories() const;
    QStringList brands(const QString &category = {}) const;
    QStringList models(const QString &brand) const;
    QStringList engines(const QString &brand, const QString &model) const;

    // Gearbox queries
    QStringList gearboxes(const QString &brand) const;

    // Search across all data (for autocomplete)
    QStringList searchEcu(const QString &query, int maxResults = 20) const;
    QStringList searchModels(const QString &query, int maxResults = 20) const;

    // ECU detection from ROM content
    QString detectEcuFromRom(const QByteArray &rom) const;

    // Extract ECU software/part numbers from ROM binary
    // Scans for common Bosch/Siemens/Continental part number patterns
    QStringList extractSwNumbers(const QByteArray &rom) const;

private:
    VehicleDatabase();
    void load();

    QVector<EcuEntry> m_ecus;
    QVector<ModelEntry> m_models;
    QVector<GearboxEntry> m_gearboxes;
    bool m_loaded = false;
};
