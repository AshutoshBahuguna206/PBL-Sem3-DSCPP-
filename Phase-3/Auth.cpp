#include "Auth.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

namespace {

// PBKDF2 with HMAC-SHA256, one 32-byte block (RFC 8018):
//   U1 = HMAC(password, salt + 00 00 00 01),  Ui = HMAC(password, U(i-1)),  result = U1 xor U2 xor ... xor Uc
QByteArray pbkdf2(const QByteArray& password, const QByteArray& salt, int iterations) {
    QByteArray block = salt;
    block.append('\0'); block.append('\0'); block.append('\0'); block.append('\1');
    QByteArray u = QMessageAuthenticationCode::hash(block, password, QCryptographicHash::Sha256);
    QByteArray t = u;
    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
        for (int j = 0; j < t.size(); ++j) t[j] = static_cast<char>(t[j] ^ u[j]);
    }
    return t;
}

}  // namespace

namespace Auth {

std::string makeSalt() {
    QByteArray salt;
    for (int i = 0; i < 4; ++i) {                      // 4 x 4 bytes from the operating system's random source
        const quint32 v = QRandomGenerator::system()->generate();
        salt.append(reinterpret_cast<const char*>(&v), 4);
    }
    return salt.toHex().toStdString();
}

std::string hashPassword(const std::string& password, const std::string& saltHex, int iterations) {
    const QByteArray salt = QByteArray::fromHex(QByteArray::fromStdString(saltHex));
    return pbkdf2(QByteArray::fromStdString(password), salt, iterations).toHex().toStdString();
}

bool verify(const std::string& password, const std::string& saltHex, const std::string& expectedHashHex) {
    const std::string actual = hashPassword(password, saltHex);
    if (actual.size() != expectedHashHex.size()) return false;
    unsigned char diff = 0;                            // compare every character, so timing does not reveal where it differs
    for (size_t i = 0; i < actual.size(); ++i) diff |= static_cast<unsigned char>(actual[i] ^ expectedHashHex[i]);
    return diff == 0;
}

}  // namespace Auth
