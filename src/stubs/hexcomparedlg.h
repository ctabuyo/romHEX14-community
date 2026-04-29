#pragma once
#include <QDialog>
#include <QList>
class Project;
class HexCompareDlg : public QDialog {
    
public:
    template<typename... Args>
    explicit HexCompareDlg(Args&&...) {}
};
