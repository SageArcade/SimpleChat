#pragma once

#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <functional>
#include <string>

namespace simplechat::networking {

using ClientId = std::uint64_t;

class WebSocketServer {
public:
    using OnConnect    = std::function<void(ClientId)>;
    using OnDisconnect = std::function<void(ClientId)>;
    using OnMessage    = std::function<void(ClientId, const std::string&)>;

    WebSocketServer(boost::asio::io_context& ioc, unsigned short port);
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;            
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    void set_on_connect(OnConnect cb);
    void set_on_disconnect(OnDisconnect cb);
    void set_on_message(OnMessage cb);

    void start();  // start accepting
    void stop();   // stop accepting + close active sessions

    // Send to a client (optional for now; useful for "server push")
    void send(ClientId client, const std::string& msg);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace simplechat::networking