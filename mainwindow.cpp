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

    if (!socket->waitForConnected()){
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
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // displacement
    ui->customplot->graph(4)->setPen(QPen(Qt::cyan));
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // velocity
    ui->customplot->graph(5)->setPen(QPen(Qt::darkBlue));

    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // Hidden graph; Represents tap count = 1
    ui->customplot->graph(6)->setPen(QPen(Qt::transparent));
    ui->customplot->graph(6)->setBrush(QBrush(QColor(147, 175, 250, 100)));
    ui->customplot->addGraph(ui->customplot->xAxis, ui->customplot->yAxis); // Hidden graph; Represents tap count = 2
    ui->customplot->graph(7)->setPen(QPen(Qt::transparent));
    ui->customplot->graph(7)->setBrush(QBrush(QColor(147, 250, 194, 100)));

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s");
    ui->customplot->xAxis->setTicker(timeTicker);
    ui->customplot->axisRect()->setupFullAxesBox();
    ui->customplot->yAxis->setRange(-1.0, 1.0);

    connect(ui->customplot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->customplot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->customplot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->customplot->yAxis2, SLOT(setRange(QCPRange)));

    ui->customplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_12);
    socketStream.setByteOrder(QDataStream::LittleEndian);

    // setup a timer thatr repeatedly calls MainWindow::realtimeDataSlot:
    dataTimer = new QTimer(this);
    connect(dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
    dataTimer->start(10);

    // connect button signals/slots
    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(stopTimer()));
    connect(ui->resumeButton, SIGNAL(clicked()), this, SLOT(resumeTimer()));
    connect(ui->checkBox_ax, SIGNAL(clicked(bool)), this, SLOT(showWhichPlots(bool)));
    connect(ui->checkBox_ay, SIGNAL(clicked(bool)), this, SLOT(showWhichPlots(bool)));
    connect(ui->checkBox_az, SIGNAL(clicked(bool)), this, SLOT(showWhichPlots(bool)));
    connect(ui->checkBox_alen, SIGNAL(clicked(bool)), this, SLOT(showWhichPlots(bool)));
    connect(ui->checkBox_disp, SIGNAL(clicked(bool)), this, SLOT(showWhichPlots(bool)));
    connect(ui->checkBox_vel, SIGNAL(clicked(bool)), this, SLOT(showWhichPlots(bool)));
    connect(ui->showRawInput, SIGNAL(clicked()), this, SLOT(showRawInput()));

    // set initial states of visibility
    ui->textBrowser_receivedMessages->setVisible(false);
    ui->customplot->graph(3)->setVisible(false);
    ui->customplot->graph(5)->setVisible(false);
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
    QByteArray socket_buffer;

    uint16_t displacement_raw = 0;
    int16_t velocity_raw = 0;
    socket_buffer = socket->readAll();
    //qDebug() << socket_buffer;
    QList<QByteArray> tempList = socket_buffer.split('\xAA');           // Split based on 1st header
    tempList.removeFirst();
    if (!tempList.isEmpty() && tempList[0].front() == '\x86') {         // If 2nd header present - packet is likely ok
        if (tempList[0].length() == 13) {
            if (tempList[0].back() == calculateChecksum(tempList[0])) {     // If checksum is correct - packet 100% ok
                memcpy(&acl_raw, tempList[0].data() + 1, 6);
                memcpy(&displacement_raw, tempList[0].data() + 7, 2);
                memcpy(&velocity_raw, tempList[0].data() + 9, 2);
                memcpy(&tap_count, tempList[0].data() + 11, 1);
                acl_x = ((float)acl_raw.x / 1.0e4);
                acl_y = ((float)acl_raw.y / 1.0e4);
                acl_z = ((float)acl_raw.z / 1.0e4);
                acl_len = sqrt((acl_x * acl_x) + (acl_y * acl_y) + (acl_z * acl_z));
                displacement = (float)((float)displacement_raw / 1.0e6);
                velocity = (float)((float)velocity_raw / 1.0e6);
            }
        }
    }

    socket_buffer.clear();
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

    // change background color based on tap count
    switch (tap_count) {
//        case 1: // enable bg color representing tap count = 1
//            ui->customplot->graph(1)->setPen(QPen(Qt::blue));
//            ui->customplot->graph(7)->setPen(QPen(Qt::transparent));
//        break;
//        case 2: // enable bg color representing tap count = 2
//            ui->customplot->graph(7)->setPen(QPen(Qt::green));
//            ui->customplot->graph(6)->setPen(QPen(Qt::transparent));
//        break;
//        default: // disable both bg
//            ui->customplot->graph(6)->setPen(QPen(Qt::transparent));
//            ui->customplot->graph(7)->setPen(QPen(Qt::transparent));
//        break;
    }

    //::::::::::::::::::: Add points to graphs :::::::::::::::::::::::::::
    if (key-lastPointKey > 0.01) // at most add point every 1 ms
    {
      // add data to lines:
      ui->customplot->graph(0)->addData(key, acl_x);
      ui->customplot->graph(1)->addData(key, acl_y);
      ui->customplot->graph(2)->addData(key, acl_z);
      ui->customplot->graph(3)->addData(key, acl_len);
      ui->customplot->graph(4)->addData(key, displacement);
      ui->customplot->graph(5)->addData(key, velocity);
      switch (tap_count) {
      case 1:
          ui->customplot->graph(6)->addData(key, 5);
          ui->customplot->graph(7)->addData(key, 0);
          break;
      case 2:
          ui->customplot->graph(7)->addData(key, 5);
          ui->customplot->graph(6)->addData(key, 0);
          break;
      default:
          ui->customplot->graph(6)->addData(key, 0);
          ui->customplot->graph(7)->addData(key, 0);
          break;
      }
      lastPointKey = key;
    }

    // make key axis range scroll with the data (at a constant range size of 8):
    ui->customplot->xAxis->setRange(key, 8, Qt::AlignRight);

    static double perSecondKey;
    static double perMinuteKey;
    ui->label->setText(QString("Tap/Second: %1").arg(tap_count));

    if (key - perSecondKey >= 1) {     // how many taps we made for this second
        inner_tap_count = inner_tap_count + tap_count;
        perSecondKey = key;
    }
    if (key - perMinuteKey > 15) {   // every 15 seconds we can estimate taps per minute
      ui->label_2->setText(QString("Tap/Minute: %1").arg(inner_tap_count * 4));
      inner_tap_count = 0;
      perMinuteKey = key;
    }

    // redraw
    ui->customplot->replot();
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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Hide/Show plots
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::showWhichPlots(bool checked)
{
    if (QObject::sender() == ui->checkBox_ax) {
        if (checked)
            ui->customplot->graph(0)->setVisible(true);
        else
            ui->customplot->graph(0)->setVisible(false);
    }
    else if (QObject::sender() == ui->checkBox_ay) {
        if (checked)
            ui->customplot->graph(1)->setVisible(true);
        else
            ui->customplot->graph(1)->setVisible(false);
    }
    else if (QObject::sender() == ui->checkBox_az) {
        if (checked)
            ui->customplot->graph(2)->setVisible(true);
        else
            ui->customplot->graph(2)->setVisible(false);
    }
    else if (QObject::sender() == ui->checkBox_alen) {
        if (checked)
            ui->customplot->graph(3)->setVisible(true);
        else
            ui->customplot->graph(3)->setVisible(false);
    }
    else if (QObject::sender() == ui->checkBox_disp) {
        if (checked)
            ui->customplot->graph(4)->setVisible(true);
        else
            ui->customplot->graph(4)->setVisible(false);
    }
    else if (QObject::sender() == ui->checkBox_vel) {
        if (checked)
            ui->customplot->graph(5)->setVisible(true);
        else
            ui->customplot->graph(5)->setVisible(false);
    }
    ui->customplot->replot();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Hide/Show raw values for x y z axes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void MainWindow::showRawInput()
{
    if (!ui->textBrowser_receivedMessages->isVisible())
        ui->textBrowser_receivedMessages->setVisible(true);
    else if (ui->textBrowser_receivedMessages->isVisible())
        ui->textBrowser_receivedMessages->setVisible(false);
}

char MainWindow::calculateChecksum(const QByteArray &buff)
{
    unsigned char checksum = 0;
    int16_t buff_len = buff.length();
    for (int i = 1; i < buff_len - 1; i++)  // start from 1 - ignoring the header
        checksum = checksum ^ buff.at(i);
    return checksum;
}
