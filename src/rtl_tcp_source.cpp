#include "rtl_tcp_client.h"

namespace rtltcp {
    Client::Client(std::shared_ptr<net::Socket> sock, dsp::stream<dsp::complex_t>* stream) {
        this->sock = sock;
        this->stream = stream;

        // Start worker
        workerThread = std::thread(&Client::worker, this);
    }

    Client::~Client() {
        close();
    }

    bool Client::isOpen() {
        return sock->isOpen();
    }

    void Client::close() {
        sock->close();
        stream->stopWriter();
        if (workerThread.joinable()) {
            workerThread.join();
        }
        stream->clearWriteStop();
    }

    void Client::worker() {
        uint8_t* buffer = dsp::buffer::alloc<uint8_t>(STREAM_BUFFER_SIZE*2);

        while (true) {
            // Read data
            int count = sock->recv(buffer, bufferSize * 2, true);
            if (count <= 0) { break; }

            // Convert to complex float
            int scount = count/2;
            for (int i = 0; i < scount; i++) {
                stream->writeBuf[i].re = ((double)buffer[i * 2] - 128.0) / 128.0;
                stream->writeBuf[i].im = ((double)buffer[(i * 2) + 1] - 128.0) / 128.0;
            }

            // Swap buffer
            if (!stream->swap(scount)) { break; }
        }

        dsp::buffer::free(buffer);
    }

    std::shared_ptr<Client> connect(dsp::stream<dsp::complex_t>* stream, std::string host, int port) {
        auto sock = net::connect(host, port);
        return std::make_shared<Client>(sock, stream);
    }
}