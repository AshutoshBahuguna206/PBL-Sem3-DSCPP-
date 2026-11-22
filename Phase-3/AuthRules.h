#pragma once
// The rules for usernames and passwords. Plain C++ (no Qt), so they are tested with the engine tests.
#include <cctype>
#include <string>

namespace AuthRules {

enum class Problem { None, UsernameTooShort, UsernameTooLong, UsernameBadCharacter, PasswordTooShort, PasswordTooLong };

constexpr size_t kMinUsername = 3, kMaxUsername = 20, kMinPassword = 6, kMaxPassword = 64;

// "  Alice " and "alice" are the same player: trim the spaces and use lower case.
inline std::string normalizeUsername(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    std::string out = s.substr(a, b - a);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Call this on the normalised name. Letters, digits and underscore only, 3 to 20 characters.
inline Problem checkUsername(const std::string& name) {
    if (name.size() < kMinUsername) return Problem::UsernameTooShort;
    if (name.size() > kMaxUsername) return Problem::UsernameTooLong;
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) && u < 128) && c != '_') return Problem::UsernameBadCharacter;
    }
    return Problem::None;
}

inline Problem checkPassword(const std::string& pw) {
    if (pw.size() < kMinPassword) return Problem::PasswordTooShort;
    if (pw.size() > kMaxPassword) return Problem::PasswordTooLong;
    return Problem::None;
}

inline const char* describe(Problem p) {
    switch (p) {
        case Problem::None: return "";
        case Problem::UsernameTooShort: return "The username needs at least 3 characters.";
        case Problem::UsernameTooLong: return "The username can have at most 20 characters.";
        case Problem::UsernameBadCharacter: return "Use only letters, digits and the underscore in the username.";
        case Problem::PasswordTooShort: return "The password needs at least 6 characters.";
        case Problem::PasswordTooLong: return "The password can have at most 64 characters.";
    }
    return "";
}

}  // namespace AuthRules
