/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "debug/DebugRpc.h"

#ifdef RX14_DEBUG_RPC

#include "debug/DebugLog.h"
#include "logger.h"
#include "mainwindow.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>

DebugRpc::DebugRpc(MainWindow *mw, QObject *parent)
    : QObject(parent), m_mw(mw)
{
}

DebugRpc::~DebugRpc()
{
    stop();
}

bool DebugRpc::start(quint16 port)
{
    if (m_server) return true;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &DebugRpc::onNewConnection);
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qCWarning(catRpc) << "listen failed on" << port
                          << ":" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    m_port = m_server->serverPort();
    qCInfo(catRpc) << "listening on 127.0.0.1:" << m_port;
    return true;
}

void DebugRpc::stop()
{
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        if (it.key()) it.key()->disconnectFromHost();
    }
    m_buffers.clear();
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_port = 0;
}

void DebugRpc::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        if (!sock) continue;
        m_buffers.insert(sock, QByteArray());
        connect(sock, &QTcpSocket::readyRead,
                this, &DebugRpc::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &DebugRpc::onClientDisconnected);
        qCDebug(catRpc) << "client connected from"
                        << sock->peerAddress().toString()
                        << ":" << sock->peerPort();
    }
}

void DebugRpc::onClientReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock) return;

    QByteArray &buf = m_buffers[sock];
    buf.append(sock->readAll());

    // Process all complete lines in the buffer
    int nl;
    while ((nl = buf.indexOf('\n')) != -1) {
        QByteArray line = buf.left(nl);
        buf.remove(0, nl + 1);
        // Trim trailing CR (Windows clients)
        while (!line.isEmpty() && (line.back() == '\r' || line.back() == ' '))
            line.chop(1);
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        QJsonObject response;

        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            response.insert("ok", false);
            response.insert("error",
                QStringLiteral("invalid JSON: %1").arg(err.errorString()));
        } else {
            response = dispatch(doc.object());
        }

        QByteArray out = QJsonDocument(response).toJson(QJsonDocument::Compact);
        out.append('\n');
        sock->write(out);
        sock->flush();
    }
}

void DebugRpc::onClientDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock) return;
    m_buffers.remove(sock);
    sock->deleteLater();
    qCDebug(catRpc) << "client disconnected";
}

QJsonObject DebugRpc::dispatch(const QJsonObject &request)
{
    const QString cmd = request.value("cmd").toString();
    const QJsonObject args = request.value("args").toObject();
    const QJsonValue id = request.value("id");

    qCDebug(catRpc) << "cmd=" << cmd << "args=" << args;

    QJsonObject resp;
    if      (cmd == "ping")        resp = cmdPing(args);
    else if (cmd == "state")       resp = cmdState(args);
    else if (cmd == "screenshot")  resp = cmdScreenshot(args);
    else if (cmd == "action")      resp = cmdAction(args);
    else if (cmd == "scroll")      resp = cmdScroll(args);
    else if (cmd == "switch_view") resp = cmdSwitchView(args);
    else if (cmd == "tail")        resp = cmdTail(args);
    else if (cmd == "help")        resp = cmdHelp(args);
    else if (cmd == "load_rom")    resp = cmdLoadRom(args);
    else if (cmd == "apply_edit")  resp = cmdApplyEdit(args);
    else if (cmd == "add_anno")    resp = cmdAddAnnotation(args);
    else if (cmd == "list_anno")   resp = cmdListAnnotations(args);
    else if (cmd == "export_maps") resp = cmdExportMapList(args);
    else if (cmd == "find_similar") resp = cmdFindSimilar(args);
    else if (cmd == "open_project") resp = cmdOpenProject(args);
    else if (cmd == "map_list")     resp = cmdMapList(args);
    else if (cmd == "read_bytes")   resp = cmdReadBytes(args);
    else if (cmd == "bulk_edit")    resp = cmdBulkEdit(args);
    else if (cmd == "undo")         resp = cmdUndo(args);
    else if (cmd == "remove_anno")  resp = cmdRemoveAnno(args);
    else {
        resp.insert("ok", false);
        resp.insert("error", QStringLiteral("unknown cmd: %1").arg(cmd));
    }

    if (!id.isUndefined()) resp.insert("id", id);
    return resp;
}

// ── Handlers ────────────────────────────────────────────────────────────────

QJsonObject DebugRpc::cmdPing(const QJsonObject &)
{
    QJsonObject r;
    r.insert("ok", true);
    r.insert("result", QStringLiteral("pong"));
    return r;
}

QJsonObject DebugRpc::cmdHelp(const QJsonObject &)
{
    QJsonObject r;
    r.insert("ok", true);
    QJsonArray cmds;
    cmds.append("ping                       — connectivity check");
    cmds.append("state                      — full UI snapshot (projects/MDI/sync/scroll)");
    cmds.append("screenshot {target?}       — PNG to D:/rx14-debug/screenshots; target ∈ {main,active,subwindow:N}");
    cmds.append("action {trigger}           — trigger named QAction (m_actSyncCursors, m_actCompare, ...)");
    cmds.append("scroll {target,sub,value}  — set scroll; target ∈ {hex,wave}; sub = subwindow index; value = row|byteOffset");
    cmds.append("switch_view {sub,index}    — set ProjectView stack index (0=Text/Hex, 1=2D/Wave, 2=3D, ...)");
    cmds.append("tail {lines?=200}          — last N lines from rx14.log");
    cmds.append("help                       — this list");
    r.insert("result", cmds);
    return r;
}

QJsonObject DebugRpc::cmdState(const QJsonObject &)
{
    QJsonObject r;
    if (!m_mw) {
        r.insert("ok", false);
        r.insert("error", "no MainWindow");
        return r;
    }
    r.insert("ok", true);
    r.insert("result", m_mw->debugStateSnapshot());
    return r;
}

QJsonObject DebugRpc::cmdScreenshot(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) {
        r.insert("ok", false);
        r.insert("error", "no MainWindow");
        return r;
    }
    QString target = args.value("target").toString("main");
    QString path;
    QString err;
    if (m_mw->debugTakeScreenshot(target, &path, &err)) {
        r.insert("ok", true);
        QJsonObject result;
        result.insert("path", path);
        r.insert("result", result);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdAction(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) {
        r.insert("ok", false);
        r.insert("error", "no MainWindow");
        return r;
    }
    const QString trigger = args.value("trigger").toString();
    if (trigger.isEmpty()) {
        r.insert("ok", false);
        r.insert("error", "missing 'trigger'");
        return r;
    }
    QString err;
    bool checked = false;
    if (m_mw->debugTriggerAction(trigger, &checked, &err)) {
        r.insert("ok", true);
        QJsonObject result;
        result.insert("trigger", trigger);
        result.insert("checked", checked);
        r.insert("result", result);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdScroll(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) {
        r.insert("ok", false);
        r.insert("error", "no MainWindow");
        return r;
    }
    const QString target = args.value("target").toString();
    const int sub = args.value("sub").toInt(-1);
    const int value = args.value("value").toInt(-1);
    if (target.isEmpty() || sub < 0 || value < 0) {
        r.insert("ok", false);
        r.insert("error", "need target ∈ {hex,wave}, sub≥0, value≥0");
        return r;
    }
    QString err;
    if (m_mw->debugSetScroll(target, sub, value, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdSwitchView(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) {
        r.insert("ok", false);
        r.insert("error", "no MainWindow");
        return r;
    }
    const int sub = args.value("sub").toInt(-1);
    const int idx = args.value("index").toInt(-1);
    if (sub < 0 || idx < 0) {
        r.insert("ok", false);
        r.insert("error", "need sub≥0, index≥0");
        return r;
    }
    QString err;
    if (m_mw->debugSwitchView(sub, idx, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdLoadRom(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) {
        r.insert("ok", false);
        r.insert("error", "no MainWindow");
        return r;
    }
    const QString path = args.value("path").toString();
    if (path.isEmpty()) {
        r.insert("ok", false);
        r.insert("error", "missing 'path'");
        return r;
    }
    QString err;
    if (m_mw->debugLoadRom(path, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdApplyEdit(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub   = args.value("sub").toInt(-1);
    int op    = args.value("op").toInt(-1);
    int start = args.value("start").toInt(-1);
    int end   = args.value("end").toInt(-1);
    double v  = args.value("value").toDouble(0.0);
    if (sub < 0 || op < 0 || start < 0 || end <= start) {
        r.insert("ok", false);
        r.insert("error", "need sub>=0, op>=0, start>=0, end>start");
        return r;
    }
    QString err;
    if (m_mw->debugApplyEdit(sub, op, start, end, v, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdAddAnnotation(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub      = args.value("sub").toInt(0);
    qint64 addr  = static_cast<qint64>(args.value("addr").toDouble(-1));
    qint64 len   = static_cast<qint64>(args.value("length").toDouble(1));
    QString text = args.value("text").toString();
    if (addr < 0) {
        r.insert("ok", false);
        r.insert("error", "need addr >= 0");
        return r;
    }
    QString err;
    if (m_mw->debugAddAnnotation(sub, addr, text, len, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdListAnnotations(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub = args.value("sub").toInt(0);
    QJsonObject result;
    result.insert("items", m_mw->debugAnnotationList(sub));
    r.insert("ok", true);
    r.insert("result", result);
    return r;
}

QJsonObject DebugRpc::cmdExportMapList(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub          = args.value("sub").toInt(0);
    bool csv         = args.value("format").toString("csv") == "csv";
    QString path     = args.value("path").toString();
    if (path.isEmpty()) {
        r.insert("ok", false);
        r.insert("error", "missing 'path'");
        return r;
    }
    QString err;
    if (m_mw->debugExportMapList(sub, csv, path, &err)) {
        r.insert("ok", true);
        QJsonObject result;
        result.insert("path", path);
        r.insert("result", result);
    } else {
        r.insert("ok", false);
        r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdReadBytes(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub      = args.value("sub").toInt(0);
    qint64 addr  = static_cast<qint64>(args.value("addr").toDouble(-1));
    int len      = args.value("len").toInt(16);
    if (addr < 0) {
        r.insert("ok", false); r.insert("error", "need addr >= 0"); return r;
    }
    QString hex = m_mw->debugReadBytes(sub, addr, len);
    if (hex.isEmpty()) {
        r.insert("ok", false); r.insert("error", "out of range or no project");
        return r;
    }
    QJsonObject result;
    result.insert("hex", hex);
    r.insert("ok", true);
    r.insert("result", result);
    return r;
}

QJsonObject DebugRpc::cmdBulkEdit(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub      = args.value("sub").toInt(0);
    int op       = args.value("op").toInt(-1);
    double v     = args.value("value").toDouble(0.0);
    QStringList names;
    for (const auto &x : args.value("maps").toArray())
        names.append(x.toString());
    if (op < 0 || names.isEmpty()) {
        r.insert("ok", false); r.insert("error", "need op>=0 and non-empty maps[]");
        return r;
    }
    QString err;
    if (m_mw->debugBulkEdit(sub, names, op, v, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false); r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdUndo(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub = args.value("sub").toInt(0);
    QString err;
    if (m_mw->debugUndo(sub, &err)) r.insert("ok", true);
    else { r.insert("ok", false); r.insert("error", err); }
    return r;
}

QJsonObject DebugRpc::cmdRemoveAnno(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub     = args.value("sub").toInt(0);
    qint64 addr = static_cast<qint64>(args.value("addr").toDouble(-1));
    if (addr < 0) {
        r.insert("ok", false); r.insert("error", "need addr"); return r;
    }
    QString err;
    if (m_mw->debugRemoveAnnotation(sub, addr, &err)) r.insert("ok", true);
    else { r.insert("ok", false); r.insert("error", err.isEmpty() ? QString("no anno at addr") : err); }
    return r;
}

QJsonObject DebugRpc::cmdOpenProject(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    QString path = args.value("path").toString();
    if (path.isEmpty()) {
        r.insert("ok", false); r.insert("error", "missing 'path'"); return r;
    }
    QString err;
    if (m_mw->debugOpenProject(path, &err)) {
        r.insert("ok", true);
    } else {
        r.insert("ok", false); r.insert("error", err);
    }
    return r;
}

QJsonObject DebugRpc::cmdMapList(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub = args.value("sub").toInt(0);
    int limit = args.value("limit").toInt(0);
    QJsonObject result;
    result.insert("maps", m_mw->debugMapList(sub, limit));
    r.insert("ok", true);
    r.insert("result", result);
    return r;
}

QJsonObject DebugRpc::cmdFindSimilar(const QJsonObject &args)
{
    QJsonObject r;
    if (!m_mw) { r.insert("ok", false); r.insert("error", "no MainWindow"); return r; }
    int sub          = args.value("sub").toInt(0);
    QString refName  = args.value("ref").toString();
    double threshold = args.value("threshold").toDouble(0.8);
    if (refName.isEmpty()) {
        r.insert("ok", false);
        r.insert("error", "missing 'ref' map name");
        return r;
    }
    QJsonObject result;
    result.insert("matches", m_mw->debugFindSimilar(sub, refName, threshold));
    r.insert("ok", true);
    r.insert("result", result);
    return r;
}

QJsonObject DebugRpc::cmdTail(const QJsonObject &args)
{
    const int n = args.value("lines").toInt(200);
    QStringList lines = Logger::instance().tail(n);
    QJsonArray arr;
    for (const auto &l : lines) arr.append(l);
    QJsonObject r;
    r.insert("ok", true);
    QJsonObject result;
    result.insert("lines", arr);
    r.insert("result", result);
    return r;
}

#endif // RX14_DEBUG_RPC
