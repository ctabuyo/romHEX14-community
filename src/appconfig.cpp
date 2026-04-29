/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "appconfig.h"
#include <QSettings>

AppConfig::AppConfig() { applyDefaults(colors); }

AppConfig &AppConfig::instance()
{
    static AppConfig s;
    return s;
}

void AppConfig::applyDefaults(AppColors &c)
{
    c.mapBand[0] = QColor(255, 58,  58);
    c.mapBand[1] = QColor(58,  150, 255);
    c.mapBand[2] = QColor(58,  255, 130);
    c.mapBand[3] = QColor(255, 200, 58);
    c.mapBand[4] = QColor(180, 58,  255);

    c.waveRow[0] = QColor(58,  180, 255);
    c.waveRow[1] = QColor(255, 160, 50);
    c.waveRow[2] = QColor(90,  210, 90);
    c.waveRow[3] = QColor(220, 80,  80);
    c.waveRow[4] = QColor(170, 100, 255);
    c.waveRow[5] = QColor(60,  210, 190);
    c.waveRow[6] = QColor(255, 220, 60);
    c.waveRow[7] = QColor(200, 130, 80);

    c.hexBg         = QColor(7,   11,  20);
    c.hexText       = QColor(231, 238, 252);
    c.hexModified   = QColor(245, 158, 11);
    c.hexSelected   = QColor(58,  145, 208);
    c.hexOffset     = QColor(107, 127, 163);
    c.hexHeaderBg   = QColor(15,  22,  41);
    c.hexHeaderText = QColor(58,  145, 208);
    c.hexBarDefault = QColor(115, 125, 140);

    // Map overlay (no-heat / classic terminal style)
    c.mapCellBg       = QColor(0,   0,   160);  // solid blue (classic)
    c.mapCellText     = QColor(255, 255, 255);  // white text
    c.mapCellModified = QColor(255, 255, 0);    // yellow for modified
    c.mapGridLine     = QColor(0,   0,   120);  // darker blue grid
    c.mapAxisXBg      = QColor(12,  32,  48);   // deep ocean teal
    c.mapAxisXText    = QColor(100, 210, 240);  // bright cyan
    c.mapAxisYBg      = QColor(38,  24,  12);   // warm bronze/brown
    c.mapAxisYText    = QColor(240, 180, 100);  // warm amber/gold

    c.waveBg         = QColor(8,  14,  26);
    c.waveGridMajor  = QColor(38, 62,  100);
    c.waveGridMinor  = QColor(22, 38,  62);
    c.waveLine       = QColor(80, 160, 255);
    c.waveOverviewBg = QColor(5,  10,  22);

    c.uiBg       = QColor(8,   11,  16);
    c.uiPanel    = QColor(22,  27,  34);
    c.uiBorder   = QColor(48,  54,  61);
    c.uiText     = QColor(201, 209, 217);
    c.uiTextDim  = QColor(139, 148, 158);
    c.uiAccent   = QColor(58,  145, 208);
}

void AppConfig::load()
{
    QSettings s("CT14", "RX14");
    AppColors def; applyDefaults(def);

    auto rd = [&](const QString &key, const QColor &fb) -> QColor {
        QString v = s.value(key).toString();
        if (v.isEmpty()) return fb;
        QColor c(v); return c.isValid() ? c : fb;
    };

    for (int i = 0; i < 5; ++i)
        colors.mapBand[i] = rd(QString("colors/mapBand%1").arg(i), def.mapBand[i]);
    for (int i = 0; i < 8; ++i)
        colors.waveRow[i] = rd(QString("colors/waveRow%1").arg(i), def.waveRow[i]);

    colors.hexBg         = rd("colors/hexBg",         def.hexBg);
    colors.hexText       = rd("colors/hexText",       def.hexText);
    colors.hexModified   = rd("colors/hexModified",   def.hexModified);
    colors.hexSelected   = rd("colors/hexSelected",   def.hexSelected);
    colors.hexOffset     = rd("colors/hexOffset",     def.hexOffset);
    colors.hexHeaderBg   = rd("colors/hexHeaderBg",   def.hexHeaderBg);
    colors.hexHeaderText = rd("colors/hexHeaderText", def.hexHeaderText);
    colors.hexBarDefault = rd("colors/hexBarDefault", def.hexBarDefault);

    colors.mapCellBg       = rd("colors/mapCellBg",       def.mapCellBg);
    colors.mapCellText     = rd("colors/mapCellText",     def.mapCellText);
    colors.mapCellModified = rd("colors/mapCellModified", def.mapCellModified);
    colors.mapGridLine     = rd("colors/mapGridLine",     def.mapGridLine);
    colors.mapAxisXBg      = rd("colors/mapAxisXBg",      def.mapAxisXBg);
    colors.mapAxisXText    = rd("colors/mapAxisXText",     def.mapAxisXText);
    colors.mapAxisYBg      = rd("colors/mapAxisYBg",      def.mapAxisYBg);
    colors.mapAxisYText    = rd("colors/mapAxisYText",     def.mapAxisYText);

    colors.waveBg         = rd("colors/waveBg",         def.waveBg);
    colors.waveGridMajor  = rd("colors/waveGridMajor",  def.waveGridMajor);
    colors.waveGridMinor  = rd("colors/waveGridMinor",  def.waveGridMinor);
    colors.waveLine       = rd("colors/waveLine",       def.waveLine);
    colors.waveOverviewBg = rd("colors/waveOverviewBg", def.waveOverviewBg);

    colors.uiBg      = rd("colors/uiBg",      def.uiBg);
    colors.uiPanel   = rd("colors/uiPanel",   def.uiPanel);
    colors.uiBorder  = rd("colors/uiBorder",  def.uiBorder);
    colors.uiText    = rd("colors/uiText",    def.uiText);
    colors.uiTextDim = rd("colors/uiTextDim", def.uiTextDim);
    colors.uiAccent  = rd("colors/uiAccent",  def.uiAccent);
    showLongMapNames = s.value("display/showLongMapNames", true).toBool();

    int pm = s.value("aiassistant/permissionMode",
                     static_cast<int>(PermissionMode::Ask)).toInt();
    if (pm < 0 || pm > 2) pm = static_cast<int>(PermissionMode::Ask);
    aiPermissionMode = static_cast<PermissionMode>(pm);
}

void AppConfig::save()
{
    QSettings s("CT14", "RX14");
    auto wr = [&](const QString &key, const QColor &c) {
        s.setValue(key, c.name(QColor::HexArgb));
    };

    for (int i = 0; i < 5; ++i) wr(QString("colors/mapBand%1").arg(i), colors.mapBand[i]);
    for (int i = 0; i < 8; ++i) wr(QString("colors/waveRow%1").arg(i), colors.waveRow[i]);

    wr("colors/hexBg",         colors.hexBg);
    wr("colors/hexText",       colors.hexText);
    wr("colors/hexModified",   colors.hexModified);
    wr("colors/hexSelected",   colors.hexSelected);
    wr("colors/hexOffset",     colors.hexOffset);
    wr("colors/hexHeaderBg",   colors.hexHeaderBg);
    wr("colors/hexHeaderText", colors.hexHeaderText);
    wr("colors/hexBarDefault", colors.hexBarDefault);

    wr("colors/mapCellBg",       colors.mapCellBg);
    wr("colors/mapCellText",     colors.mapCellText);
    wr("colors/mapCellModified", colors.mapCellModified);
    wr("colors/mapGridLine",     colors.mapGridLine);
    wr("colors/mapAxisXBg",      colors.mapAxisXBg);
    wr("colors/mapAxisXText",    colors.mapAxisXText);
    wr("colors/mapAxisYBg",      colors.mapAxisYBg);
    wr("colors/mapAxisYText",    colors.mapAxisYText);

    wr("colors/waveBg",         colors.waveBg);
    wr("colors/waveGridMajor",  colors.waveGridMajor);
    wr("colors/waveGridMinor",  colors.waveGridMinor);
    wr("colors/waveLine",       colors.waveLine);
    wr("colors/waveOverviewBg", colors.waveOverviewBg);

    wr("colors/uiBg",      colors.uiBg);
    wr("colors/uiPanel",   colors.uiPanel);
    wr("colors/uiBorder",  colors.uiBorder);
    wr("colors/uiText",    colors.uiText);
    wr("colors/uiTextDim", colors.uiTextDim);
    wr("colors/uiAccent",  colors.uiAccent);
    s.setValue("display/showLongMapNames", showLongMapNames);
    s.setValue("aiassistant/permissionMode", static_cast<int>(aiPermissionMode));

    s.sync();
}

void AppConfig::resetToDefaults()
{
    applyDefaults(colors);
    emit colorsChanged();
}
