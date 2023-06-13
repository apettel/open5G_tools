#include <array>
#include <iostream>
#include <complex>
#include <vector>
#include <QDebug>
#include <iio.h>
#include "iio_buf_reader.h"

void Iio_buf_reader::init_iio_device()
{
    // Initialize libiio context
    ctx = iio_create_context_from_uri("ip:192.168.137.2");
    if (!ctx) {
        std::cerr << "Failed to create libiio context" << std::endl;
        return;
    }
    stop_read_thread = false;
}

void Iio_buf_reader::close_iio_device()
{
    iio_context_destroy(ctx);
    qDebug("iio_context_destroyed");
    ctx = nullptr;
}

bool Iio_buf_reader::is_open()
{
    return buffer != nullptr;
}

unsigned int iio_sample_size = 0;
const size_t iio_buffer_size = 610 * 14; // reads 4 subframes
void Iio_buf_reader::open_iio_buffer()
{
    if (buffer != nullptr)
    {
        qDebug("buffer is already open!");
        return;
    }

    unsigned int timeout_ms = 30000;
    iio_context_set_timeout(ctx, timeout_ms);

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
        iio_context_destroy(ctx);
        return;
    }

    // bool blocking = false;
    // int ret = iio_buffer_set_blocking_mode(buffer, blocking);
    // if (ret != 0)
    // {
    //     char buf[256];
    //     iio_strerror(-(int)ret, buf, sizeof(buf));
    //     std::cerr << "Failed to set blocking = false, code: " << ret << " error: " << buf << std::endl;
    // }
    qDebug("iio buffer created");
    stop_read_thread = false;
    read_thread = std::make_unique<std::thread>(&Iio_buf_reader::read_iio_buffer, this);
    iio_state = Iio_state::open;
}

void Iio_buf_reader::close_iio_buffer()
{
    stop_read_thread = true;
    qDebug("iio_buffer_cancel");
    iio_buffer_cancel(buffer);
    read_thread->join();
    qDebug("iio_buffer_destroy");
    iio_buffer_destroy(buffer);
    buffer = nullptr;
    dev = nullptr;
    iio_state = Iio_state::closed;
}

void Iio_buf_reader::read_iio_buffer()
{
    std::cout << "thread launched" << std::endl;
    while (!stop_read_thread)
    {
        int ret = iio_buffer_refill(buffer);
        if (ret < 0) {
            if (ret == -EAGAIN)
                std::cout << "no data in buffer" << std::endl;
            else
            {
                char buf[256];
                iio_strerror(-(int)ret, buf, sizeof(buf));
                std::cerr << "Failed to refill buffer, error: " << buf << std::endl;
            }
        }
        else
        {
            // qDebug("rx %d bytes", ret);
            if (ret != iio_buffer_size * iio_sample_size)
                qDebug("Buffer not full!");
            unsigned char *start = static_cast<unsigned char *>(iio_buffer_start(buffer));

            if (subframes.size() % 20 == 0)
            {
                unsigned int index = 0;
                for (unsigned int m = 0; m < 4; m++)
                {
                    unsigned int exponent = start[index];
                    if (exponent > 16)
                        exponent = 10;
                    unsigned int dummy = start[index + 1];
                    index += 2;
                    unsigned long timestamp = 0;
                    for (unsigned int l = 0; l < 8; l++)
                        timestamp += static_cast<unsigned int>(start[index + l]) << (8 * l);
                    index += 8;
                    unsigned long delta_timestamp = timestamp - old_timestamp;
                    qDebug("exponent %d, dummy %d, timestamp: %lu, delta_t: %lu", exponent, dummy, timestamp, delta_timestamp);
                    old_timestamp = timestamp;
                    if ((delta_timestamp != 548) && (delta_timestamp != 613300) && (delta_timestamp != 613304))
                    {
                        // qDebug("timing error!");
                        // index -= 10;
                        // qDebug("%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x", 
                        //     start[index + 0], start[index + 1], start[index + 2], start[index + 3], start[index + 4], start[index + 5], start[index + 6], start[index + 7], start[index + 8], start[index + 9]);
                        // index += 10;
                    }
                    const float factor = (2 ^ 16) / (2 ^ exponent);
                    std::complex<float> data[300];
                    for (unsigned int i = 0; i < 300; i++)
                    {
                        //qDebug("%d %d", static_cast<char>(start[index]), static_cast<char>(start[index + 1]));
                        data[i] = std::complex<float>(static_cast<char>(start[index]) * factor, static_cast<char>(start[index + 1]) * factor);
                        index += 2;
                    }
                    if (m == 0)
                    {
                        // ComplexDataEvent dataEvent(&data[30], 240);
                        // scatterWidget->customEvent(&dataEvent);
                        std::vector<std::complex<float>> data_vector;
                        data_vector.assign(&data[30], &data[30] + 240);
                        data_callback(data_vector);
                    }
                }
            }

            subframes.push_back(std::array<char, 604>());
        }
        // logEdit->append(QString("read ") + QString::number(ret) + QString(" bytes"));
        // std::cout << "." << std::endl;
    }
    std::cout << "thread terminated" << std::endl;
}