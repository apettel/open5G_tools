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
#include <QTime>
#include <array>
#include <iostream>
#include <atomic>
#include <thread>
#include <functional>
#include "srsgui/srsgui++.h"
#include "srsgui/common/Events.h"
#include "iio_buf_reader.h"

std::unique_ptr<QTextEdit> logEdit;
std::unique_ptr<QTimer> timer;
std::unique_ptr<QPushButton> connectButton;
std::unique_ptr<QPushButton> connectCellButton;
std::unique_ptr<QLineEdit> fsStatusEdit;
std::unique_ptr<QLineEdit> numDisconnectsEdit;
std::unique_ptr<QLineEdit> rgfOverflowEdit;
std::unique_ptr<QLineEdit> cellidEdit;
std::unique_ptr<QTcpSocket> socket;
std::unique_ptr<ScatterWidget> scatterWidget;
std::unique_ptr<Iio_buf_reader> iio_buf_reader;

struct ReadItem
{
    unsigned int addr;
    std::function<void(int)> handler;
};

const unsigned int ADDR_FS_STATE = 0x7C44C014;
const unsigned int ADDR_N_ID = 0x7C448020;
const unsigned int ADDR_NUM_DISCONNECTS = 0x7C44C034;
const unsigned int ADDR_RGF_OVERFLOW = 0x7c44C01C;

std::array<ReadItem, 4> read_items = {
    ReadItem{.addr = ADDR_FS_STATE, .handler = [](auto val) { fsStatusEdit->setText(QString::number(val)); }},
    ReadItem{.addr = ADDR_N_ID, .handler = [](auto val) { cellidEdit->setText(QString::number(val)); }},
    ReadItem{.addr = ADDR_NUM_DISCONNECTS, .handler = [](auto val) { numDisconnectsEdit->setText(QString::number(val)); }},
    ReadItem{.addr = ADDR_RGF_OVERFLOW, .handler = [](auto val) { rgfOverflowEdit->setText(QString::number(val)); }}};

bool busy = false;
enum Connect_state {
    not_connected,
    connected
};
Connect_state connect_state = Connect_state::not_connected;

void connect();
void connectToCell();

void delay( int millisecondsToWait )
{
    QTime dieTime = QTime::currentTime().addMSecs( millisecondsToWait );
    while( QTime::currentTime() < dieTime )
    {
        QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
    }
}

unsigned int num_disconnects;
unsigned int num_disconnects_old = 0;

void update_status()
{
    if (busy)
    {
        qDebug("busy!");
        return;
    }

    busy = true;
    // logEdit->append("update");
    
    if(socket->state() != QAbstractSocket::ConnectedState)
    {
        qDebug("Not connected!");
        busy = false;
        return;
    }
    
    QByteArray data;
    const auto num_read_bytes = read_items.size() * 4 + 1;
    data.append(QByteArray::fromHex("00000000"));
    data[0] = num_read_bytes & 0xFF;
    data[1] = (num_read_bytes >> 8) & 0xFF;
    data.append(QByteArray::fromHex("00"));
    for (auto read_item : read_items)
    {
        QByteArray data2 = QByteArray::fromHex("00000000");
        data2[0] = read_item.addr & 0xFF;
        data2[1] = (read_item.addr >> 8) & 0xFF;
        data2[2] = (read_item.addr >> 16) & 0xFF;
        data2[3] = (read_item.addr >> 24) & 0xFF;
        data.append(data2);
    }
    // logEdit->append(QString("sending ") + data.toHex());
    socket->write(data);
    socket->waitForBytesWritten();
    QByteArray rx_data;
    unsigned int try_cnt = 0;
    while ((rx_data.size() < (read_items.size() * 4 + 5)) && try_cnt < 100)
    {
        if (socket->bytesAvailable() > 0)
        {
            rx_data.append(socket->readAll());
            // logEdit->append(QString("receive ") + rx_data.toHex());
        }
        try_cnt++;
    }
    
    if (rx_data.size() != read_items.size() * 4 + 5)
    {
        qDebug("Did not receive all data!");
        busy = false;
        return;
    }
    
    auto rx_data_ptr = rx_data.begin();
    std::advance(rx_data_ptr, 5);
    for (auto read_item : read_items)
    {
        unsigned int val = static_cast<unsigned char>(*(rx_data_ptr++));
        val += static_cast<unsigned char>(*(rx_data_ptr++)) << 8;
        val += static_cast<unsigned char>(*(rx_data_ptr++)) << 16;
        val += static_cast<unsigned char>(*(rx_data_ptr++)) << 24;
        read_item.handler(val);
        if (read_item.addr == ADDR_FS_STATE)
        {
            if (val == 2)
            {
                if (connect_state != Connect_state::connected)
                {
                    connect_state = Connect_state::connected;
                    qDebug("connected");
                }
            }
            else if (val == 0)
            {
                if (connect_state == Connect_state::connected)
                {
                    connect_state = Connect_state::not_connected;
                    qDebug("disconnected");
                }
            }
        }
        else if (read_item.addr == ADDR_NUM_DISCONNECTS)
            num_disconnects = val;
    }
    busy = false;
    if ((fsStatusEdit->text() != QString("2")) && (num_disconnects > num_disconnects_old))
    {
        qDebug("connecting ....");
        if (iio_buf_reader->is_open())
        {
            iio_buf_reader->close_iio_buffer();
        }
        else
            qDebug("buffer already closed!");

        iio_buf_reader->open_iio_buffer();
        num_disconnects_old = num_disconnects;
        connectToCell();
    }
    logEdit->append(QString("received buffers ") + QString::number(iio_buf_reader->subframes.size()));
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
        iio_buf_reader->init_iio_device();
        iio_buf_reader->open_iio_buffer();
        // init_iio_device();
        // open_iio_buffer();
        // if (iio_state == Iio_state::open)
        // {
        //     stop_read_thread = false;
        //     read_thread = std::make_unique<std::thread>(read_iio_buffer);
        // }
        timer->start(200);
        connectButton->setText("disconnect");
    }
}

void connectToCell()
{
    if (socket->state() != QAbstractSocket::ConnectedState)
        return;
    if (fsStatusEdit->text() != QString("0"))
        return;
    QByteArray data;
    data.append(QByteArray::fromHex("09000000"));
    data.append(QByteArray::fromHex("01"));
    data.append(QByteArray::fromHex("18c0447c"));  //  0x7C44C018
    data.append(QByteArray::fromHex("01000000"));
    // logEdit->append(QString("sending ") + data.toHex());
    socket->write(data);
    socket->waitForBytesWritten();
}

void data_callback(std::vector<std::complex<float>> data)
{
    ComplexDataEvent dataEvent(&data[0], 240);
    scatterWidget->customEvent(&dataEvent);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    socket = std::make_unique<QTcpSocket>();
    iio_buf_reader = std::make_unique<Iio_buf_reader>();
    iio_buf_reader->data_callback = &data_callback;

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

    connectButton = std::make_unique<QPushButton>("connect");
    settingsLayout.addWidget(connectButton.get(), 3, 0);
    QObject::connect(connectButton.get(), &QPushButton::clicked, [&]() {
        connect();
    });

    connectCellButton = std::make_unique<QPushButton>("connect to cell");
    settingsLayout.addWidget(connectCellButton.get(), 3, 1);
    QObject::connect(connectCellButton.get(), &QPushButton::clicked, [&]() {
        connectToCell();
    });

    QGroupBox logGroupBox("Log");
    layout.addWidget(&logGroupBox, 1, 0);
    QGridLayout logLayout(&logGroupBox);
    logEdit = std::make_unique<QTextEdit>();
    logLayout.addWidget(logEdit.get());

    QGroupBox statusGroupBox("lower phy status");
    layout.addWidget(&statusGroupBox, 0, 1);
    QGridLayout statusLayout(&statusGroupBox);
    // fs_status
    label = new QLabel("fs_status");
    statusLayout.addWidget(label, 0, 0);
    fsStatusEdit = std::make_unique<QLineEdit>("");
    statusLayout.addWidget(fsStatusEdit.get(), 0, 1);
    // num_disconnects
    label = new QLabel("num_disconnects");
    statusLayout.addWidget(label, 1, 0);
    numDisconnectsEdit = std::make_unique<QLineEdit>("");
    statusLayout.addWidget(numDisconnectsEdit.get(), 1, 1);
    // N_id
    label = new QLabel("N_id");
    statusLayout.addWidget(label, 2, 0);
    cellidEdit = std::make_unique<QLineEdit>("");
    statusLayout.addWidget(cellidEdit.get(), 2, 1);
    // N_id
    label = new QLabel("rgf_overflow");
    statusLayout.addWidget(label, 3, 0);
    rgfOverflowEdit = std::make_unique<QLineEdit>("");
    statusLayout.addWidget(rgfOverflowEdit.get(), 3, 1);

    QGroupBox pbchRawGroupBox("PBCH raw");
    layout.addWidget(&pbchRawGroupBox, 1, 1);
    QGridLayout pbchRawLayout(&pbchRawGroupBox);

    scatterWidget = std::make_unique<ScatterWidget>();
    pbchRawLayout.addWidget(scatterWidget.get(), 0, 0);
    scatterWidget->setWidgetXAxisAutoScale(false);
    scatterWidget->setWidgetYAxisAutoScale(false);
    scatterWidget->setWidgetXAxisScale(-300, 300);
    scatterWidget->setWidgetYAxisScale(-300, 300);

    QGroupBox pbchCorrectedGroupBox("PBCH corrected");
    layout.addWidget(&pbchCorrectedGroupBox, 2, 1);
    QGridLayout pbchCorrectedLayout(&pbchCorrectedGroupBox);

    timer = std::make_unique<QTimer>();
    QObject::connect(timer.get(), &QTimer::timeout, [&]() {
        update_status();
    });

    mainWindow.show();

    return app.exec();
}
