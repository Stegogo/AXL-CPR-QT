#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAbstractSocket>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHostAddress>
#include <QMessageBox>
#include <QMetaType>
#include <QString>
#include <QStandardPaths>
#include <QTcpSocket>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
signals:
    void newMessage(QString);
private slots:
    void readSocket();
    void discardSocket();
    void displayError(QAbstractSocket::SocketError socketError);
    void realtimeDataSlot();

    void displayMessage(const QString& str);
private:
    Ui::MainWindow *ui;

    QTcpSocket* socket;
    QTimer* dataTimer;

    bool display_ax = true;
    bool display_ay = true;
    bool display_az = true;
    bool display_alen = true;

    float acl_x = 0.0f;
    float acl_y = 0.0f;
    float acl_z = 0.0f;
    float acl_len = 0.0f;

    typedef struct {
        int16_t x;
        int16_t y;
        int16_t z;
    } acl_raw_t;
    acl_raw_t acl_raw;
};

#endif // MAINWINDOW_H
