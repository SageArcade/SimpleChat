#pragma once
#include <chrono>
#include <string>

namespace simplechat::networking {

struct Session {
    using Clock = std::chrono::steady_clock;

    std::string client_id;  // "client-<ulid>"
    std::string user_id;     // "user-<ulid>"
    std::string room_id;     // "room-<id>"

    Clock::time_point connected_at{};
    Clock::time_point last_seen{};

    void touch() noexcept { last_seen = Clock::now(); }
};

} // namespace simplechat::networking