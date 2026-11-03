#pragma once
// Issue #4: salted, iterated password hashing (PBKDF2-HMAC-SHA256, self-contained).
// For a real deployment prefer libsodium/Argon2; this keeps the project dependency-free.
#include <cstdint>
#include <string>
#include <vector>

#include "result.hpp"
#include "rng.hpp"

namespace tms {

using Bytes = std::vector<std::uint8_t>;

constexpr unsigned kDefaultIterations = 10000;

Bytes toBytes(const std::string& s);
std::string toHex(const Bytes& b);
Result<Bytes> fromHex(const std::string& hex);

Bytes sha256(const Bytes& data);
Bytes hmacSha256(const Bytes& key, const Bytes& message);
Bytes pbkdf2HmacSha256(const Bytes& password, const Bytes& salt, unsigned iterations);  // 32 bytes

std::string generateSaltHex(SeededRng& rng);  // 16 random bytes, hex
std::string hashPasswordHex(const std::string& password, const std::string& saltHex, unsigned iterations);
bool verifyPassword(const std::string& password, const std::string& saltHex,
                    const std::string& expectedHashHex, unsigned iterations);  // constant-time compare

}  // namespace tms
