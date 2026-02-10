#include "networking/WebSocketServer.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp> 

#include <unordered_set>
#include <unordered_map>
#include <iostream>

static constexpr bool DEBUG_MODE = true;
namespace json = boost::json;

static std::string guest_name(simplechat::networking::ClientId id) {
    return "guest-" + std::to_string(id);
}

static std::string dump(const json::object& obj) {
    return json::serialize(obj);
}

static void send_debug(simplechat::networking::WebSocketServer &server,
                       simplechat::networking::ClientId id,
                       const json::object &payload) {
    if constexpr (DEBUG_MODE){
        server.send(id, dump(payload));
    }
}

int main() {
    using namespace simplechat::networking;

    boost::asio::io_context ioc;

    std::unordered_set<ClientId> clients;
    std::unordered_map<ClientId, std::string> user_of;
    std::unordered_map<ClientId, std::string> room_of; // default "lobby"

    unsigned short port = 9002;
    WebSocketServer server(ioc, port);

    server.set_on_connect([&](ClientId id) {
        clients.insert(id);
        user_of[id] = guest_name(id);
        room_of[id] = "lobby";

        // welcome to this client only
        server.send(id, dump({{"type","system"}, {"text","welcome to SimpleChat"}, {"room","lobby"}}));

        // notify everyone in lobby
        json::object evt{{"type","system"}, {"text", user_of[id] + " joined lobby"}, {"room","lobby"}};
        for (auto c : clients) {
            if (room_of[c] == "lobby") server.send(c, dump(evt));
        }
    });

    server.set_on_disconnect([&](ClientId id) {
        auto user = user_of.count(id) ? user_of[id] : guest_name(id);
        auto room = room_of.count(id) ? room_of[id] : "lobby";

        clients.erase(id);
        user_of.erase(id);
        room_of.erase(id);

        json::object evt{{"type","system"}, {"text", user + " left " + room}, {"room", room}};
        for (auto c : clients) {
            if (room_of[c] == room) server.send(c, dump(evt));
        } 
    });

    server.set_on_message([&](ClientId id, const std::string &msg)
                          {
        json::value v;
        try {
            v = json::parse(msg);
        } catch (...) {
            server.send(id, dump({{"type","error"}, {"text","invalid json"}}));
            return;
        }

        auto* obj = v.if_object();
        if (!obj || !obj->if_contains("type")) {
            server.send(id, dump({{"type","error"}, {"text","missing type"}}));
            return;
        }

        std::string type = json::value_to<std::string>((*obj)["type"]);

        if (type == "join") {
            // read optional room/user
            if (obj->if_contains("user")) user_of[id] = json::value_to<std::string>((*obj)["user"]);
            if (obj->if_contains("room")) room_of[id] = json::value_to<std::string>((*obj)["room"]);

            // Debug reply (sender-only)
            server.send(id, dump({
                {"type", "debug_join"},
                {"client_id", id},
                {"user", user_of[id]},
                {"room", room_of[id]}
            }));

            auto text = user_of[id] + " joined " + room_of[id];
            json::object evt{{"type","system"}, {"text", text}, {"room", room_of[id]}};

            for (auto c : clients) {
                if (room_of[c] == room_of[id]) server.send(c, dump(evt));
            }
        }
        else if (type == "msg") {
            if (!obj->if_contains("text")) {
                server.send(id, dump({{"type","error"}, {"text","missing text"}}));
                return;
            }
            std::string text = json::value_to<std::string>((*obj)["text"]);

            // Debug reply (sender-only)
            server.send(id, dump({
                {"type", "debug_msg"},
                {"client_id", id},
                {"user", user_of[id]},
                {"room", room_of[id]},
                {"text", text}
            }));

            json::object out{
                {"type","msg"},
                {"from", user_of[id]},
                {"room", room_of[id]},
                {"text", text}
            };

            for (auto c : clients) {
                if (room_of[c] == room_of[id]) server.send(c, dump(out));
            }
        }
        else {
            server.send(id, dump({{"type","error"}, {"text","unknown type"}}));
        } 
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