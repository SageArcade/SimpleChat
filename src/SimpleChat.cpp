#include "networking/WebSocketServer.h"
#include "networking/Session.hpp"
#include "chat/Room.h"
#include "chat/User.h"
#include "chat/IDGenerator.hpp"


#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp> 

#include <unordered_set>
#include <unordered_map>
#include <iostream>

namespace json = boost::json;


static constexpr bool DEBUG_MODE = true;
static constexpr std::string_view kLobbyRoomId = "room-lobby";

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

    simplechat::chat::IDGenerator idgen;

    std::unordered_set<ClientId> clients;
    std::unordered_map<ClientId, simplechat::networking::Session> sessions;
    std::unordered_map<std::string, simplechat::chat::User> users;

    unsigned short port = 9002;
    WebSocketServer server(ioc, port);

    server.set_on_connect([&](ClientId client_id) {
        clients.insert(client_id);

        simplechat::chat::User user{idgen, "guest", std::string(kLobbyRoomId)};
        const std::string user_id = user.user_id();
        users.emplace(user_id, std::move(user));

        Session sess;
        sess.client_id = idgen.clientID();
        sess.user_id   = user_id;
        sess.room_id   = std::string(kLobbyRoomId);

        sessions.emplace(client_id, std::move(sess));

        auto& current_session = sessions.at(client_id);
        auto& current_user = users.at(current_session.user_id);

        // 3) Welcome message (to this client only)
        server.send(client_id, dump({
            {"type", "system"},
            {"text", "welcome to SimpleChat"},
            {"client_id", current_session.client_id},
            {"user_id", current_user.user_id()},
            {"room_id", current_session.room_id}
        }));

        // 4) Notify everyone in lobby
        json::object evt{
            {"type", "system"},
            {"text", current_user.name() + " joined lobby"},
            {"user_id", current_user.user_id()},
            {"room_id", current_session.room_id}
        };

        for (ClientId c : clients) {
            auto it = sessions.find(c);
            if (it != sessions.end() && it->second.room_id == current_session.room_id) {
                server.send(c, dump(evt));
            }
        }

    });

    server.set_on_disconnect([&](ClientId client_id) {
        
        std::string room_id = "room-lobby";
        std::string username = "guest";

        auto session_iterator = sessions.find(client_id);
        if (session_iterator != sessions.end()) {
            room_id = session_iterator->second.room_id;

            auto user_iterator = users.find(session_iterator->second.user_id);
            if (user_iterator != users.end()) username = user_iterator->second.name();
        }

        clients.erase(client_id);
        sessions.erase(client_id);
        // NOTE: don't erase user blindly if you later allow multiple sessions/user
        // For now (1 session/user), you can erase user too:
        if (session_iterator != sessions.end()) users.erase(session_iterator->second.user_id);

        json::object evt{{"type","system"},
                        {"text", username + " left " + room_id},
                        {"room_id", room_id}};

        for (auto c : clients) {
            auto it = sessions.find(c);
            if (it != sessions.end() && it->second.room_id == room_id) {
                server.send(c, dump(evt));
            }
        } 
    });

    server.set_on_message([&](ClientId id, const std::string &msg) {
        auto sit = sessions.find(id);
        if (sit == sessions.end()) {
            server.send(id, dump({{"type","error"}, {"text","unknown session"}}));
            return;
        }
        auto& sess = sit->second;
    
        auto uit = users.find(sess.user_id);
        if (uit == users.end()) {
            server.send(id, dump({{"type","error"}, {"text","unknown user"}}));
            return;
        }
        auto& user = uit->second;
    
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
            // Only allow setting display name for now
            if (obj->if_contains("user")) {
                user.set_name(json::value_to<std::string>((*obj)["user"]));
            }
    
            // Debug reply (sender-only)
            server.send(id, dump({
                {"type", "debug_join"},
                {"client_id", sess.client_id},
                {"user_id", user.user_id()},
                {"name", user.name()},
                {"room_id", sess.room_id}
            }));
    
            json::object evt{
                {"type","system"},
                {"text", user.name() + " joined " + sess.room_id},
                {"user_id", user.user_id()},
                {"room_id", sess.room_id}
            };
    
            for (auto c : clients) {
                auto it = sessions.find(c);
                if (it != sessions.end() && it->second.room_id == sess.room_id) {
                    server.send(c, dump(evt));
                }
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
                {"client_id", sess.client_id},
                {"user_id", user.user_id()},
                {"name", user.name()},
                {"room_id", sess.room_id},
                {"text", text}
            }));
    
            json::object out{
                {"type","msg"},
                {"from", user.name()},
                {"user_id", user.user_id()},
                {"client_id", sess.client_id},
                {"room_id", sess.room_id},
                {"text", text}
            };
    
            for (auto c : clients) {
                auto it = sessions.find(c);
                if (it != sessions.end() && it->second.room_id == sess.room_id) {
                    server.send(c, dump(out));
                }
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