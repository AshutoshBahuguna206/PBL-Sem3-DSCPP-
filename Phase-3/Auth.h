#pragma once
// Password storage. A password is never stored: we store a random salt and a PBKDF2-HMAC-SHA256 hash of
// the password and the salt. PBKDF2 repeats the hash many times (kIterations), which makes guessing passwords slow.
// Needs only Qt Core.
#include <string>

namespace Auth {

constexpr int kIterations = 100000;

std::string makeSalt();                                                       // 16 random bytes, as 32 hex characters
std::string hashPassword(const std::string& password, const std::string& saltHex, int iterations = kIterations);
bool verify(const std::string& password, const std::string& saltHex, const std::string& expectedHashHex);

}  // namespace Auth
