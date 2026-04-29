#pragma once
#include <QDialog>
#include <QByteArray>
#include <QString>
#include "checksummanager.h"
class ChecksumSelectDlg : public QDialog {
    
public:
    explicit ChecksumSelectDlg(const QByteArray & = {}, const QString & = {}, QWidget *p = nullptr) : QDialog(p) {}
    ChecksumDllInfo selectedDll() const { return {}; }
};
