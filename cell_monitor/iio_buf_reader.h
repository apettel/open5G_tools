#include <thread>
#include <atomic>
#include <vector>
#include <complex>
#include <functional>

enum Iio_state {
    open,
    closed
};

class Iio_buf_reader
{
    public:
        void init_iio_device();
        void close_iio_device();
        void open_iio_buffer();
        void close_iio_buffer();
        void read_iio_buffer();
        bool is_open();

        std::atomic_bool stop_read_thread;
        std::function<void(std::vector<std::complex<float>>)> data_callback;
        std::vector<std::array<char, 604>> subframes;

    private:
        std::unique_ptr<std::thread> read_thread;
        struct iio_context *ctx = nullptr;
        struct iio_device *dev = nullptr;
        struct iio_buffer *buffer = nullptr;
        Iio_state iio_state = Iio_state::closed;
        unsigned long old_timestamp = 0;

};