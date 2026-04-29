#pragma once
#include <QObject>
#include <QString>
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *p = nullptr) : QObject(p) {}
    void checkForUpdates(bool = false) {}
    static QString currentVersion() { return {}; }
signals:
    void updateAvailable(const QString &ver, const QString &changelog, const QString &url);
    void noUpdateAvailable();
    void checkFailed(const QString &err);
};
