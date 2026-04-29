#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
struct TranslationResult { QString name, translation, description; bool cached = false; };
class ApiClient : public QObject {
    Q_OBJECT
public:
    static ApiClient &instance() { static ApiClient a; return a; }
    bool isLoggedIn() const { return false; }
    bool hasModule(const QString &) const { return false; }
    QString userEmail() const { return {}; }
    QStringList modules() const { return {}; }
    void refreshEntitlements() {}
    void translateMap(const QString &, const QString &, const QString &,
                      std::function<void(bool, const QString &, const QString &)> cb) { if (cb) cb(false, {}, {}); }
    void translateMapsBatch(const QVector<QPair<QString,QString>> &, const QString &,
                            std::function<void(int,int)>, std::function<void(const QVector<TranslationResult>&)> cb) { if (cb) cb({}); }
signals:
    void loginStateChanged();
};
