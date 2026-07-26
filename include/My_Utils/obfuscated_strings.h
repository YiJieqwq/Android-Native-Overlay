#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace linuxbkr::obfuscation {

// Compile-time XOR encoding for UI-only strings. The clear text is reconstructed
// only on first use and retained for the lifetime of the process because ImGui
// consumes labels synchronously. This is a cost-raising measure, not a secret
// storage mechanism: anything rendered can still be observed dynamically.
template <std::size_t N, std::uint8_t Key>
class ObfuscatedString {
public:
    std::array<std::uint8_t, N> bytes{};

    constexpr explicit ObfuscatedString(const char (&plain)[N]) : bytes{} {
        for (std::size_t i = 0; i < N; ++i)
            bytes[i] = static_cast<std::uint8_t>(plain[i]) ^ Key;
    }

    std::string decode() const {
        std::string out;
        out.resize(N ? N - 1 : 0);
        for (std::size_t i = 0; i + 1 < N; ++i)
            out[i] = static_cast<char>(bytes[i] ^ Key);
        return out;
    }
};

template <std::uint8_t Key, std::size_t N>
constexpr ObfuscatedString<N, Key> make(const char (&plain)[N]) {
    return ObfuscatedString<N, Key>(plain);
}

} // namespace linuxbkr::obfuscation

#define LBK_TEXT_IMPL(s, id) ([]() -> const char * { \
    constexpr std::uint8_t key = static_cast<std::uint8_t>( \
        0x5A ^ (((id) * 29u + sizeof(s) * 17u) & 0xFFu)); \
    static constexpr auto encoded = ::linuxbkr::obfuscation::make<key>(s); \
    static const std::string decoded = encoded.decode(); \
    return decoded.c_str(); \
}())
#define LBK_TEXT(s) LBK_TEXT_IMPL(s, __COUNTER__)
