#pragma once

#include <cstdint>
#include <chrono>
#include <string>


#include "chat/IDGenerator.hpp"


namespace simplechat::chat {

class User {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t kMaxNameLen = 24;

    // Main constructor: generates user-<id>
    User(simplechat::chat::IDGenerator& idgen,
         std::string name,
         std::string room = "lobby");

    // Optional: restore from existing ID (DB / tests / reconnect)
    User(std::string user_id,
         std::string name,
         std::string room = "lobby");

    const std::string& user_id() const noexcept;
    const std::string& name() const noexcept;
    const std::string& room() const noexcept;

    void set_name(std::string new_name);
    void set_room(std::string new_room);

    Clock::time_point connected_at() const noexcept;
    Clock::time_point last_seen() const noexcept;
    void touch() noexcept;

private:
    static std::string sanitize_name(std::string s);
    static std::string sanitize_room(std::string s);
    static std::string trim_copy(std::string s);
    static bool is_space(char c) noexcept;

private:
    std::string user_id_;
    std::string name_;
    std::string room_;
    Clock::time_point connected_at_;
    Clock::time_point last_seen_;
};

} // namespace simplechat::chat