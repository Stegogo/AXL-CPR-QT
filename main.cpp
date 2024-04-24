#include "mainwindow.h"
#include "qcustomplot.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.resize(900,500);
    w.setWindowIcon(QIcon(":icons/cpr.png"));
    w.show();
    return a.exec();
}
