#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>

namespace simplechat::chat {

// ULID uses Crockford's Base32 (no I, L, O, U) => 26 chars for 128 bits.
class IDGenerator {
public:
    enum class Kind { Room, User, Client };

    IDGenerator()
        : rng_(seed_engine_()) {}

    // Main API
    std::string make(Kind kind) {
        const char* prefix = prefix_of(kind);
        return std::string(prefix) + "-" + ulid_string_();
    }

    // Convenience helpers
    std::string roomID()   { return make(Kind::Room); }
    std::string userID()   { return make(Kind::User); }
    std::string clientID() { return make(Kind::Client); }

private:
    static const char* prefix_of(Kind kind) {
        switch (kind) {
            case Kind::Room:   return "room";
            case Kind::User:   return "user";
            case Kind::Client: return "client";
        }
        return "id";
    }

    // --- ULID generation (monotonic within same millisecond) ---
    std::string ulid_string_() {
        // 16 bytes = 128 bits
        std::array<std::uint8_t, 16> bytes{};

        const std::uint64_t ts_ms = now_ms_();

        // Fill timestamp: 48 bits big-endian into bytes[0..5]
        bytes[0] = static_cast<std::uint8_t>((ts_ms >> 40) & 0xFF);
        bytes[1] = static_cast<std::uint8_t>((ts_ms >> 32) & 0xFF);
        bytes[2] = static_cast<std::uint8_t>((ts_ms >> 24) & 0xFF);
        bytes[3] = static_cast<std::uint8_t>((ts_ms >> 16) & 0xFF);
        bytes[4] = static_cast<std::uint8_t>((ts_ms >> 8)  & 0xFF);
        bytes[5] = static_cast<std::uint8_t>((ts_ms)       & 0xFF);

        // Randomness: 80 bits into bytes[6..15]
        // We make it monotonic for same-millisecond bursts.
        {
            std::lock_guard<std::mutex> lk(mu_);

            if (ts_ms != last_ts_ms_) {
                // new millisecond => fresh random 80 bits
                fill_random_80_(bytes);
                last_ts_ms_ = ts_ms;
                last_rand_ = extract_rand_80_(bytes);
            } else {
                // same millisecond => increment the previous 80-bit number
                ++last_rand_;
                write_rand_80_(bytes, last_rand_);
            }
        }

        return crockford_base32_encode_(bytes);
    }

    static std::uint64_t now_ms_() {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
        );
    }

    // --- Randomness helpers (80-bit) ---
    // Store 80-bit in a 128-bit integer (upper bits unused)
    using u128 = unsigned __int128;

    void fill_random_80_(std::array<std::uint8_t, 16>& bytes) {
        // generate 80 random bits
        std::uint64_t a = dist64_(rng_);
        std::uint64_t b = dist64_(rng_);

        // Use 64 bits from a and top 16 bits from b => 80 bits total.
        bytes[6]  = static_cast<std::uint8_t>((a >> 56) & 0xFF);
        bytes[7]  = static_cast<std::uint8_t>((a >> 48) & 0xFF);
        bytes[8]  = static_cast<std::uint8_t>((a >> 40) & 0xFF);
        bytes[9]  = static_cast<std::uint8_t>((a >> 32) & 0xFF);
        bytes[10] = static_cast<std::uint8_t>((a >> 24) & 0xFF);
        bytes[11] = static_cast<std::uint8_t>((a >> 16) & 0xFF);
        bytes[12] = static_cast<std::uint8_t>((a >> 8)  & 0xFF);
        bytes[13] = static_cast<std::uint8_t>((a)       & 0xFF);

        bytes[14] = static_cast<std::uint8_t>((b >> 56) & 0xFF);
        bytes[15] = static_cast<std::uint8_t>((b >> 48) & 0xFF);
    }

    static u128 extract_rand_80_(const std::array<std::uint8_t, 16>& bytes) {
        u128 x = 0;
        for (int i = 6; i <= 15; ++i) {
            x = (x << 8) | bytes[static_cast<std::size_t>(i)];
        }
        // x contains 80 bits (but stored in 128)
        return x;
    }

    static void write_rand_80_(std::array<std::uint8_t, 16>& bytes, u128 rand80) {
        // write 10 bytes (80 bits) into bytes[6..15] big-endian
        for (int i = 15; i >= 6; --i) {
            bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(rand80 & 0xFF);
            rand80 >>= 8;
        }
    }

    // --- Crockford base32 encoding for ULID (16 bytes -> 26 chars) ---
    static std::string crockford_base32_encode_(const std::array<std::uint8_t, 16>& bytes) {
        static constexpr char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

        // ULID encoding takes 128 bits and outputs 26 base32 chars (130 bits capacity; top 2 bits are 0)
        // We'll stream bits from the 16-byte array.
        std::string out;
        out.reserve(26);

        std::uint32_t buffer = 0;
        int bits_in_buffer = 0;

        for (std::uint8_t byte : bytes) {
            buffer = (buffer << 8) | byte;
            bits_in_buffer += 8;

            while (bits_in_buffer >= 5) {
                int shift = bits_in_buffer - 5;
                std::uint8_t index = static_cast<std::uint8_t>((buffer >> shift) & 0x1F);
                out.push_back(alphabet[index]);
                bits_in_buffer -= 5;

                // keep only remaining bits
                buffer &= (1u << bits_in_buffer) - 1u;
            }
        }

        if (bits_in_buffer > 0) {
            // pad remaining bits to 5
            std::uint8_t index = static_cast<std::uint8_t>((buffer << (5 - bits_in_buffer)) & 0x1F);
            out.push_back(alphabet[index]);
        }

        // ULID requires exactly 26 chars. Our method can produce 26 for 128 bits + padding.
        // If it produced 27 due to padding edge-case, trim. If 25 (shouldn't), pad with '0'.
        if (out.size() > 26) out.resize(26);
        while (out.size() < 26) out.push_back('0');

        return out;
    }

    static std::mt19937_64 seed_engine_() {
        // Seed with multiple entropy sources
        std::random_device rd;
        std::seed_seq seq{
            rd(), rd(), rd(), rd(),
            static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(&rd))
        };
        return std::mt19937_64(seq);
    }

private:
    std::mt19937_64 rng_;
    std::uniform_int_distribution<std::uint64_t> dist64_{0, ~std::uint64_t(0)};

    std::mutex mu_;
    std::uint64_t last_ts_ms_ = 0;
    u128 last_rand_ = 0;
};

} // namespace simplechat::chat