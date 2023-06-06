#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QDebug>
#include <QGroupBox>
#include <QTimer>
#include <QTextEdit>
#include <QTcpSocket>

QTextEdit *logEdit;
QTimer *timer;
QPushButton *connectButton;
QLineEdit *fsStatusEdit;
QLineEdit *numDisconnectsEdit;
QLineEdit *cellidEdit;
QTcpSocket *socket;

void update_status()
{
    logEdit->append("update");
    
    if(socket->state() != QAbstractSocket::ConnectedState)
    {
        qDebug("Not connected!");
        return;
    }
    
    QByteArray data;
    unsigned int num_req_items = 3;
    unsigned int num_read_bytes = num_req_items * 4 + 1;
    data.append(QByteArray::fromHex("00000000"));
    data[0] = num_read_bytes && 0xFF;
    data.append(QByteArray::fromHex("00"));
    for (int i = 0; i < num_req_items; i ++)
    {
        QByteArray data2 = QByteArray::fromHex("00000000");
        unsigned int addr = 0x7c448020;
        data2[0] = addr & 0xFF;
        data2[1] = (addr >> 8) & 0xFF;
        data2[2] = (addr >> 16) & 0xFF;
        data2[3] = (addr >> 24) & 0xFF;
        data.append(data2);
    }
    logEdit->append(QString("sending ") + data.toHex());
    qDebug("%d", data.size());
    socket->write(data);
    socket->waitForBytesWritten();
    if (socket->bytesAvailable() > 0)
    {
        QByteArray data = socket->readAll();
        logEdit->append(QString("receive ") + data.toHex());
    }
}

void connect()
{
    if (timer->isActive())
    {
        timer->stop();
        socket->disconnectFromHost();
        if (socket->state() == QAbstractSocket::UnconnectedState || socket->waitForDisconnected(1000))
            qDebug("Disconnected!");
        connectButton->setText("connect");
    }
    else
    {
        socket->connectToHost("192.168.137.2", 69);
        if (socket->waitForConnected(1000))
            qDebug("Connected!");
        timer->start(100);
        connectButton->setText("disconnect");
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    socket = new QTcpSocket();

    // Create main window and layout
    QWidget mainWindow;
    QGridLayout layout(&mainWindow);

    QGroupBox settingsGroupBox("Settings");
    layout.addWidget(&settingsGroupBox);
    QGridLayout settingsLayout(&settingsGroupBox);
    
    // ip
    QLabel *label = new QLabel("ip");
    settingsLayout.addWidget(label, 0, 0);
    QLineEdit *textField = new QLineEdit("192.168.137.2");
    settingsLayout.addWidget(textField, 0, 1);

    // port
    label = new QLabel("port");
    settingsLayout.addWidget(label, 1, 0);
    textField = new QLineEdit("69");
    settingsLayout.addWidget(textField, 1, 1);

    // update interval
    label = new QLabel("update interval [ms]");
    settingsLayout.addWidget(label, 2, 0);
    textField = new QLineEdit("200");
    settingsLayout.addWidget(textField, 2, 1);

    connectButton = new QPushButton("connect");
    settingsLayout.addWidget(connectButton, 3, 1);
    QObject::connect(connectButton, &QPushButton::clicked, [&]() {
        connect();
    });

    QGroupBox logGroupBox("Log");
    layout.addWidget(&logGroupBox, 1, 0);
    QGridLayout logLayout(&logGroupBox);
    logEdit = new QTextEdit();
    logLayout.addWidget(logEdit);

    QGroupBox statusGroupBox("lower phy status");
    layout.addWidget(&statusGroupBox, 0, 1);
    QGridLayout statusLayout(&statusGroupBox);
    // fs_status
    label = new QLabel("fs_status");
    statusLayout.addWidget(label, 0, 0);
    fsStatusEdit = new QLineEdit("");
    statusLayout.addWidget(fsStatusEdit, 0, 1);
    // num_disconnects
    label = new QLabel("num_disconnects");
    statusLayout.addWidget(label, 1, 0);
    numDisconnectsEdit = new QLineEdit("");
    statusLayout.addWidget(numDisconnectsEdit, 1, 1);
    // N_id
    label = new QLabel("N_id");
    statusLayout.addWidget(label, 2, 0);
    cellidEdit = new QLineEdit("");
    statusLayout.addWidget(cellidEdit, 2, 1);

    QGroupBox pbchRawGroupBox("PBCH raw");
    layout.addWidget(&pbchRawGroupBox, 1, 1);
    QGridLayout pbchRawLayout(&pbchRawGroupBox);

    QGroupBox pbchCorrectedGroupBox("PBCH corrected");
    layout.addWidget(&pbchCorrectedGroupBox, 2, 1);
    QGridLayout pbchCorrectedLayout(&pbchCorrectedGroupBox);

    timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, [&]() {
        update_status();
    });

    mainWindow.show();

    return app.exec();
}
