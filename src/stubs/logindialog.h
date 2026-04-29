#pragma once
#include <QDialog>
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *p = nullptr) : QDialog(p) {}
};
