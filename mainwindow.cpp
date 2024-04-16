#include "mainwindow.h"
#include "ui_mainwindow.h"

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Constructor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    socket = new QTcpSocket(this);

    connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);

    socket->connectToHost(QHostAddress("192.168.4.1"), 9000);

    if(socket->waitForConnected())
        ui->statusBar->showMessage("Connected to Server");
    else{
        QMessageBox::critical(this,"QTCPClient", QString("The following error occurred: %1.").arg(socket->errorString()));
        exit(EXIT_FAILURE);
    }

    // Setting up plot module
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // accelerometer X component
    ui->customplot->graph(0)->setPen(QPen(Qt::blue));
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // accelerometer Y component
    ui->customplot->graph(1)->setPen(QPen(Qt::red));
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // accelerometer Z component
    ui->customplot->graph(2)->setPen(QPen(Qt::darkGreen));
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // accelerometer vector length
    ui->customplot->graph(3)->setPen(QPen(Qt::green));
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s");
    ui->customplot->xAxis->setTicker(timeTicker);
    ui->customplot->axisRect()->setupFullAxesBox();
    ui->customplot->yAxis->setRange(-1.2, 1.2);

    connect(ui->customplot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->customplot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->customplot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->customplot->yAxis2, SLOT(setRange(QCPRange)));

    ui->customplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
    dataTimer = new QTimer(this);
    connect(dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
    dataTimer->start(10);

    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(stopTimer()));
    connect(ui->resumeButton, SIGNAL(clicked()), this, SLOT(resumeTimer()));
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Destructor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
MainWindow::~MainWindow()
{
    if(socket->isOpen())
        socket->close();
    delete ui;
    delete dataTimer;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Read TCP
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::readSocket()
{
    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_12);
    socketStream.setByteOrder(QDataStream::LittleEndian);

    buffer = socket->read(9);
    memcpy(&acl_raw, buffer.constData() + 2, 6);

    acl_x = (float)((float)acl_raw.x / 1.0e4);
    acl_y = (float)((float)acl_raw.y / 1.0e4);
    acl_z = (float)((float)acl_raw.z / 1.0e4);
    acl_len = sqrt((acl_x * acl_x) + (acl_y * acl_y) + (acl_z * acl_z));
    QString message = QString("x: %1 y: %2 z: %3").arg(acl_x).arg(acl_y).arg(acl_z);
    emit newMessage(message);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Disconnect case
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::discardSocket()
{
    socket->deleteLater();
    socket=nullptr;

    ui->statusBar->showMessage("Disconnected!");
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Display TCP errors
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
        break;
        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "QTCPClient", "The host was not found. Please check the host name and port settings.");
        break;
        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "QTCPClient", "The connection was refused by the peer. Make sure QTCPServer is running, and check that the host name and port settings are correct.");
        break;
        default:
            QMessageBox::information(this, "QTCPClient", QString("The following error occurred: %1.").arg(socket->errorString()));
        break;
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Plot update
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::realtimeDataSlot()
{
    static QTime time(QTime::currentTime());
    // calculate two new data points:
    double key = time.elapsed()/1000.0; // time elapsed since start of demo, in seconds
    static double lastPointKey = 0;

    // Hide/Show plots
    if (display_ax)
        ui->customplot->graph(0)->setPen(QPen(Qt::blue));
    else
        ui->customplot->graph(0)->setPen(QPen(Qt::transparent));
    if (display_ay)
        ui->customplot->graph(1)->setPen(QPen(Qt::red));
    else
        ui->customplot->graph(1)->setPen(QPen(Qt::transparent));
    if (display_az)
        ui->customplot->graph(2)->setPen(QPen(Qt::darkGreen));
    else
        ui->customplot->graph(2)->setPen(QPen(Qt::transparent));
    if (display_alen) {
        ui->customplot->graph(3)->setPen(QPen(Qt::green));}
    else {
        ui->customplot->graph(3)->setPen(QPen(Qt::transparent)); }

    //::::::::::::::::::: Add points to graphs :::::::::::::::::::::::::::
    if (key-lastPointKey > 0.002) // at most add point every 2 ms
    {
      // add data to lines:
      ui->customplot->graph(0)->addData(key, acl_x);
      ui->customplot->graph(1)->addData(key, acl_y);
      ui->customplot->graph(2)->addData(key, acl_z);
      ui->customplot->graph(3)->addData(key, acl_len);
      lastPointKey = key;
    }
    // make key axis range scroll with the data (at a constant range size of 8):
    ui->customplot->xAxis->setRange(key, 8, Qt::AlignRight);
    ui->customplot->replot();

    // update selected plots list if needed
    if (ui->checkBox_ax->isChecked()) display_ax = true;
    else if (!(ui->checkBox_ax->isChecked())) display_ax = false;
    if (ui->checkBox_ay->isChecked()) display_ay = true;
    else if (!(ui->checkBox_ay->isChecked())) display_ay = false;
    if (ui->checkBox_az->isChecked()) display_az = true;
    else if (!(ui->checkBox_az->isChecked())) display_az = false;
    if (ui->checkBox_alen->isChecked()) display_alen = true;
    else if (!(ui->checkBox_alen->isChecked())) display_alen = false;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Update raw data window
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser_receivedMessages->append(str);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Stop button handle
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::stopTimer()
{
    dataTimer->stop();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Resume button handle
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::resumeTimer()
{
    dataTimer->start(10);
}
