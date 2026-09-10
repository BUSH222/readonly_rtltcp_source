#pragma once
#include <utils/net.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <thread>

namespace rtltcp {
#pragma pack(push, 1)
        struct Command {
            uint8_t cmd;
            uint32_t param;
        };
#pragma pack(pop)

    class Client {
    public:
        Client(std::shared_ptr<net::Socket> sock, dsp::stream<dsp::complex_t>* stream);
        ~Client();

        bool isOpen();
        void close();

    private:
        void worker();

        std::shared_ptr<net::Socket> sock;
        std::thread workerThread;
        dsp::stream<dsp::complex_t>* stream;
        int bufferSize = 2400000 / 200;
    };

    std::shared_ptr<Client> connect(dsp::stream<dsp::complex_t>* stream, std::string host, int port = 1234);
}