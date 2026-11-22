// Tests for password hashing (Auth.cpp). Needs only Qt Core. Build the auth_tests target in Qt Creator and run it;
// the last line must say "Auth tests passed." The expected values below were computed with an independent
// implementation (Python's hashlib); the first two are also the published PBKDF2-HMAC-SHA256 test vectors.
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include "gui/Auth.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

int main() {
    // Known answers for PBKDF2-HMAC-SHA256 (32-byte output, salt given as hex).
    CHECK(Auth::hashPassword("password", "73616c74", 1) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK(Auth::hashPassword("password", "73616c74", 2) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    const std::string salt = "000102030405060708090a0b0c0d0e0f";
    const std::string h = Auth::hashPassword("correct horse battery staple", salt);     // default: 100000 iterations
    CHECK(h == "49d49c25f597846209f0d92e7770ab64e1c75e94b4ce6c509265ee67175d2a1e");
    CHECK(h.size() == 64);                                                               // 32 bytes as hex
    CHECK(Auth::hashPassword("Correct horse battery staple", salt) == "7b1939cdbba162f78acd67a4757edca9b7f2958f841172a69208dc18be098aa0");  // one letter differs
    CHECK(Auth::hashPassword("correct horse battery staple", "0f0e0d0c0b0a09080706050403020100") == "d3139a69dfec23ec796f3dac09d99f8805da84de5bd8e1e1ea66da04d88de4c1");  // other salt

    // Checking a password.
    CHECK(Auth::verify("correct horse battery staple", salt, h));
    CHECK(!Auth::verify("correct horse battery stapl", salt, h));                        // wrong password
    CHECK(!Auth::verify("", salt, h));                                                   // empty password
    CHECK(!Auth::verify("correct horse battery staple", salt, h.substr(0, 63)));         // hash of the wrong length
    CHECK(!Auth::verify("correct horse battery staple", "ffffffffffffffffffffffffffffffff", h));   // wrong salt

    // Salts: 32 hex characters, and different every time.
    std::set<std::string> salts;
    for (int i = 0; i < 20; ++i) {
        const std::string s = Auth::makeSalt();
        CHECK(s.size() == 32 && s.find_first_not_of("0123456789abcdef") == std::string::npos);
        salts.insert(s);
    }
    CHECK(salts.size() == 20);

    // Same password with two different salts gives two different hashes (so equal passwords are not visible).
    CHECK(Auth::hashPassword("same password", *salts.begin()) != Auth::hashPassword("same password", *salts.rbegin()));

    if (failures == 0) std::cout << "Auth tests passed.\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
