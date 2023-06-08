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
#include <iio.h>
#include <iostream>
#include <atomic>
#include <thread>
#include "srsgui/srsgui++.h"
#include "srsgui/common/Events.h"

QTextEdit *logEdit;
QTimer *timer;
QPushButton *connectButton;
QPushButton *connectCellButton;
QLineEdit *fsStatusEdit;
QLineEdit *numDisconnectsEdit;
QLineEdit *cellidEdit;
QTcpSocket *socket;
ScatterWidget *scatterWidget;

struct iio_context *ctx = nullptr;
struct iio_device *dev = nullptr;
struct iio_buffer *buffer = nullptr;

unsigned int read_addr [] = {0x7c44C014, 0x7c448020, 0x7c44C034};
unsigned int num_read_items = 3;
bool busy = false;
enum Connect_state {
    not_connected,
    connected
};
Connect_state connect_state = Connect_state::not_connected;
enum Iio_state {
    open,
    closed
};
Iio_state iio_state = Iio_state::closed;

void connect();
void connectToCell();

void init_iio_device()
{

}

unsigned int iio_sample_size = 0;
const size_t iio_buffer_size = 1024*10;
void open_iio_buffer()
{
    // Initialize libiio context
    ctx = iio_create_context_from_uri("ip:192.168.137.2");
    if (!ctx) {
        std::cerr << "Failed to create libiio context" << std::endl;
        return;
    }

    // Open the first available IIO device
    dev = iio_context_find_device(ctx, "open5G_phy");
    if (!dev) {
        std::cerr << "Failed to find IIO device" << std::endl;
        iio_context_destroy(ctx);
        return;
    }

    unsigned int nb_channels = iio_device_get_channels_count(dev);
    qDebug("iio device has %d channels", nb_channels);
    struct iio_channel *ch = iio_device_get_channel(dev, 0);
    iio_channel_enable(ch);

    iio_sample_size = iio_device_get_sample_size(dev);
	if (iio_sample_size == 0) {
		fprintf(stderr, "Unable to get sample size, returned 0\n");
		iio_context_destroy(ctx);
		return;
	} else if (iio_sample_size < 0) {
		char buf[256];
		iio_strerror(errno, buf, sizeof(buf));
		fprintf(stderr, "Unable to get sample size : %s\n", buf);
		iio_context_destroy(ctx);
		return;
	}

    // Create buffer for reading samples
    buffer = iio_device_create_buffer(dev, iio_buffer_size, false);
    if (!buffer) {
        std::cerr << "Failed to create buffer" << std::endl;
        // iio_device_destroy(dev);
        iio_context_destroy(ctx);
        return;
    }

    bool blocking = false;
    int ret = iio_buffer_set_blocking_mode(buffer, blocking);

    qDebug("iio buffer created");
    iio_state = Iio_state::open;
}

std::vector<std::array<char, 604>> subframes;
std::atomic_bool stop_read_thread(true);
void read_iio_buffer()
{
    std::cout << "thread launched" << std::endl;
    while (!stop_read_thread)
    {
        int ret = iio_buffer_refill(buffer);
        if (ret < 0) {
            if (ret == -EAGAIN)
                std::cout << "no data in buffer" << std::endl;
            else
                std::cerr << "Failed to refill buffer" << std::endl;
        }
        else
        {
            if (ret != iio_buffer_size * iio_sample_size)
                qDebug("Buffer not full!");
            unsigned char *start = static_cast<unsigned char *>(iio_buffer_start(buffer));

            if (subframes.size() == 0)
            {
                unsigned int index = 0;
                for (unsigned int m = 0; m < 2; m++)
                {
                    qDebug("%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x", 
                        start[index + 0], start[index + 1], start[index + 2], start[index + 3], start[index + 4], start[index + 5], start[index + 6], start[index + 7], start[index + 8], start[index + 9]);
                    index += 10;
                    const int factor = 10; //2 ^ (start[index + 0]);
                    std::complex<float> data[300];
                    for (unsigned int i = 0; i < 300; i++)
                    {
                        qDebug("%d %d", static_cast<char>(start[index]), static_cast<char>(start[index + 1]));
                        if (i > 100 && i < 200)
                            data[i] = std::complex<float>(static_cast<char>(start[index]) * factor, static_cast<char>(start[index + 1]) * factor);
                        else
                            data[i] = std::complex<float>(0, 0);
                        index += 2;
                    }
                    ComplexDataEvent *dataEvent = new ComplexDataEvent(&data[0], 300);
                    if (m == 0)
                        scatterWidget->customEvent(dataEvent);
                }
            }

            subframes.push_back(std::array<char, 604>());
        }
        // logEdit->append(QString("read ") + QString::number(ret) + QString(" bytes"));
        // std::cout << "." << std::endl;
    }
}
std::thread *read_thread;

void close_iio_buffer()
{
    iio_state = Iio_state::closed;
    // Cleanup
    qDebug("iio_buffer_cancel");
    iio_buffer_cancel(buffer);
    qDebug("iio_buffer_destroy");
    iio_buffer_destroy(buffer);
    // iio_device_destroy(dev);
    qDebug("iio_context_destroy");
    iio_context_destroy(ctx);
    qDebug("iio_context_destroyed");
    buffer = nullptr;
    dev = nullptr;
    ctx = nullptr;
}

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
    unsigned int num_read_bytes = num_read_items * 4 + 1;
    data.append(QByteArray::fromHex("00000000"));
    data[0] = num_read_bytes & 0xFF;
    data[1] = (num_read_bytes >> 8) & 0xFF;
    data.append(QByteArray::fromHex("00"));
    for (int i = 0; i < num_read_items; i ++)
    {
        QByteArray data2 = QByteArray::fromHex("00000000");
        data2[0] = read_addr[i] & 0xFF;
        data2[1] = (read_addr[i] >> 8) & 0xFF;
        data2[2] = (read_addr[i] >> 16) & 0xFF;
        data2[3] = (read_addr[i] >> 24) & 0xFF;
        data.append(data2);
    }
    // logEdit->append(QString("sending ") + data.toHex());
    socket->write(data);
    socket->waitForBytesWritten();
    QByteArray rx_data;
    unsigned int try_cnt = 0;
    while ((rx_data.size() < (num_read_items * 4 + 5)) && try_cnt < 100)
    {
        if (socket->bytesAvailable() > 0)
        {
            rx_data.append(socket->readAll());
            // logEdit->append(QString("receive ") + rx_data.toHex());
        }
        try_cnt++;
    }
    
    if (rx_data.size() != num_read_items * 4 + 5)
    {
        qDebug("Did not receive all data!");
        busy = false;
        return;
    }
    
    for (int i = 0; i < num_read_items; i++)
    {
        unsigned int val = static_cast<unsigned char>(rx_data[5 + 4*i]);
        val += static_cast<unsigned char>(rx_data[6 + 4*i]) << 8;
        val += static_cast<unsigned char>(rx_data[7 + 4*i]) << 16;
        val += static_cast<unsigned char>(rx_data[8 + 4*i]) << 24;
        if (i == 0)
        {
            fsStatusEdit->setText(QString::number(val));
            if (val == 2)
            {
                if (connect_state != Connect_state::connected)
                {
                    connect_state = Connect_state::connected;
                    open_iio_buffer();
                    stop_read_thread = false;
                    read_thread = new std::thread(read_iio_buffer);
                }
            }
            else if (val != 2)
            {
                if (connect_state == Connect_state::connected)
                {
                    connect_state = Connect_state::not_connected;
                    stop_read_thread = true;
                    read_thread->join();
                    subframes.clear();
                    close_iio_buffer();
                }
            }
        }
        else if (i == 1)
            cellidEdit->setText(QString::number(val));
        else if (i == 2)
            numDisconnectsEdit->setText(QString::number(val));
    }
    busy = false;
    if (fsStatusEdit->text() != QString("2"))
        connectToCell();
    logEdit->append(QString("receive buffers") + QString::number(subframes.size()));
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
        init_iio_device();
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
    settingsLayout.addWidget(connectButton, 3, 0);
    QObject::connect(connectButton, &QPushButton::clicked, [&]() {
        connect();
    });

    connectCellButton = new QPushButton("connect to cell");
    settingsLayout.addWidget(connectCellButton, 3, 1);
    QObject::connect(connectCellButton, &QPushButton::clicked, [&]() {
        connectToCell();
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

    scatterWidget = new ScatterWidget();
    pbchRawLayout.addWidget(scatterWidget, 0, 0);

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
