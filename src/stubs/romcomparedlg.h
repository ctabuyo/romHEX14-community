#pragma once
#include <QDialog>
#include <QVector>
#include <QByteArray>
#include "romdata.h"
class Project;
class RomCompareDlg : public QDialog {
    
public:
    template<typename... Args>
    explicit RomCompareDlg(Args&&...) {}
};
