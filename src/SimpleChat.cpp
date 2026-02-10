#include "networking/WebSocketServer.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>

int main() {
    using namespace simplechat::networking;

    boost::asio::io_context ioc;

    unsigned short port = 9002;
    WebSocketServer server(ioc, port);

    server.set_on_connect([](ClientId id) {
        std::cout << "[SimpleChat] client connected: " << id << "\n";
    });

    server.set_on_disconnect([](ClientId id) {
        std::cout << "[SimpleChat] client disconnected: " << id << "\n";
    });

    server.set_on_message([&server](ClientId id, const std::string& msg) {
        std::cout << "[SimpleChat] message from " << id << ": " << msg << "\n";
        // You can call server.send(id, ...) here too (server push)
    });

    server.start();

    // Graceful shutdown on Ctrl+C / SIGTERM
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        std::cout << "\n[SimpleChat] shutting down...\n";
        server.stop();
        ioc.stop();
    });

    std::cout << "[SimpleChat] WS server running on port " << port << "\n";
    ioc.run();
    std::cout << "[SimpleChat] exit.\n";
    return 0;
}