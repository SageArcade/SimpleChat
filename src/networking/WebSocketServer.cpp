#include "WebSocketServer.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace simplechat::networking {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class WebSocketServer::Impl {
public:
    Impl(asio::io_context& ioc, unsigned short port)
        : ioc_(ioc),
          acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {}

    void start() { do_accept(); }

    void stop() {
        beast::error_code ec;
        acceptor_.close(ec);

        // Close all sessions
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [id, s] : sessions_) {
            s->close();
        }
        sessions_.clear();
    }

    void send(ClientId client, const std::string& msg) {
        std::shared_ptr<class Session> s;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = sessions_.find(client);
            if (it == sessions_.end()) return;
            s = it->second;
        }
        s->send(msg);
    }

    void set_on_connect(OnConnect cb) { on_connect_ = std::move(cb); }
    void set_on_disconnect(OnDisconnect cb) { on_disconnect_ = std::move(cb); }
    void set_on_message(OnMessage cb) { on_message_ = std::move(cb); }

private:
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(Impl& server, tcp::socket socket, ClientId id)
            : server_(server),
              id_(id),
              ws_(std::move(socket)),
              strand_(asio::make_strand(server_.ioc_)) {}

        ClientId id() const { return id_; }

        void start() {
            // For minimal setup: accept immediately.
            ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

            ws_.async_accept(
                asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](beast::error_code ec) {
                        if (ec) return self->fail("accept", ec);

                        if (self->server_.on_connect_) self->server_.on_connect_(self->id_);
                        self->do_read();
                    }));
        }

        void send(const std::string& msg) {
            asio::post(
                strand_,
                [self = shared_from_this(), msg] {
                    bool writing = !self->write_queue_.empty();
                    self->write_queue_.push_back(msg);
                    if (!writing) self->do_write();
                });
        }

        void close() {
            asio::post(
                strand_,
                [self = shared_from_this()] {
                    beast::error_code ec;
                    self->ws_.close(websocket::close_code::normal, ec);
                });
        }

    private:
        void do_read() {
            ws_.async_read(
                buffer_,
                asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](beast::error_code ec, std::size_t) {
                        if (ec) return self->on_close_or_fail(ec);

                        std::string msg = beast::buffers_to_string(self->buffer_.data());
                        self->buffer_.consume(self->buffer_.size());

                        if (self->server_.on_message_) self->server_.on_message_(self->id_, msg);

                        // Minimal behavior: echo back
                        self->send("echo: " + msg);

                        self->do_read();
                    }));
        }

        void do_write() {
            ws_.text(true);
            ws_.async_write(
                asio::buffer(write_queue_.front()),
                asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](beast::error_code ec, std::size_t) {
                        if (ec) return self->on_close_or_fail(ec);

                        self->write_queue_.pop_front();
                        if (!self->write_queue_.empty()) self->do_write();
                    }));
        }

        void on_close_or_fail(beast::error_code ec) {
            // WebSocket close is common; treat it as disconnect.
            if (ec == websocket::error::closed) {
                server_.remove_session(id_);
                if (server_.on_disconnect_) server_.on_disconnect_(id_);
                return;
            }
            fail("io", ec);
            server_.remove_session(id_);
            if (server_.on_disconnect_) server_.on_disconnect_(id_);
        }

        void fail(const char* what, beast::error_code ec) {
            std::cerr << "[Session " << id_ << "] " << what << ": " << ec.message() << "\n";
        }

        Impl& server_;
        ClientId id_;

        websocket::stream<beast::tcp_stream> ws_;
        // Use the io_context executor type for compatibility with older Boost.Asio.
        asio::strand<asio::io_context::executor_type> strand_;

        beast::flat_buffer buffer_;
        std::deque<std::string> write_queue_;
    };

    void do_accept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (ec) {
                    // If acceptor closed during shutdown, ignore.
                    if (ec == asio::error::operation_aborted) return;
                    std::cerr << "[accept] " << ec.message() << "\n";
                    return do_accept();
                }

                auto id = next_client_id_++;
                auto session = std::make_shared<Session>(*this, std::move(socket), id);

                {
                    std::lock_guard<std::mutex> lk(mu_);
                    sessions_[id] = session;
                }

                session->start();
                do_accept();
            });
    }

    void remove_session(ClientId id) {
        std::lock_guard<std::mutex> lk(mu_);
        sessions_.erase(id);
    }

private:
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;

    std::atomic<ClientId> next_client_id_{1};

    std::mutex mu_;
    std::unordered_map<ClientId, std::shared_ptr<Session>> sessions_;

    OnConnect on_connect_;
    OnDisconnect on_disconnect_;
    OnMessage on_message_;
};

// ---- WebSocketServer wrapper ----

WebSocketServer::WebSocketServer(asio::io_context& ioc, unsigned short port)
    : impl_(new Impl(ioc, port)) {}

void WebSocketServer::set_on_connect(OnConnect cb) { impl_->set_on_connect(std::move(cb)); }
void WebSocketServer::set_on_disconnect(OnDisconnect cb) { impl_->set_on_disconnect(std::move(cb)); }
void WebSocketServer::set_on_message(OnMessage cb) { impl_->set_on_message(std::move(cb)); }

void WebSocketServer::start() { impl_->start(); }
void WebSocketServer::stop() { impl_->stop(); }

void WebSocketServer::send(ClientId client, const std::string& msg) { impl_->send(client, msg); }

WebSocketServer::~WebSocketServer() = default;

} // namespace simplechat::networking
