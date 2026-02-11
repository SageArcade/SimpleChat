
#include "chat/User.h"

#include <utility>

namespace simplechat::chat {

User::User(simplechat::chat::IDGenerator& idgen, std::string name, std::string room) : user_id_(idgen.userID()),
        name_(sanitize_name(std::move(name))),
        room_(sanitize_room(std::move(room))),
        connected_at_(Clock::now()),
        last_seen_(connected_at_) {}

User::User(std::string user_id, std::string name, std::string room) : user_id_(std::move(user_id)),
        name_(sanitize_name(std::move(name))),
        room_(sanitize_room(std::move(room))),
        connected_at_(Clock::now()),
        last_seen_(connected_at_) {}

const std::string& User::user_id() const noexcept { return user_id_; }
const std::string& User::name() const noexcept { return name_; }
const std::string& User::room() const noexcept { return room_; }

void User::set_name(std::string new_name) {
    name_ = sanitize_name(std::move(new_name));
}

void User::set_room(std::string new_room) {
    room_ = sanitize_room(std::move(new_room));
}

User::Clock::time_point User::connected_at() const noexcept { return connected_at_; }
User::Clock::time_point User::last_seen() const noexcept { return last_seen_; }

void User::touch() noexcept {
    last_seen_ = Clock::now();
}

bool User::is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string User::trim_copy(std::string s) {
    std::size_t start = 0;
    while (start < s.size() && is_space(s[start])) ++start;

    std::size_t end = s.size();
    while (end > start && is_space(s[end - 1])) --end;

    if (start == 0 && end == s.size()) return s;
    return s.substr(start, end - start);
}

std::string User::sanitize_name(std::string s) {
    s = trim_copy(std::move(s));

    if (s.size() > kMaxNameLen) {
        s.resize(kMaxNameLen);
        s = trim_copy(std::move(s));
    }

    if (s.empty()) s = "guest";
    return s;
}

std::string User::sanitize_room(std::string s) {
    s = trim_copy(std::move(s));
    if (s.empty()) s = "lobby";
    return s;
}

} // namespace simplechat::chat