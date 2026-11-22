#pragma once
// A tiny JSON parser (plain C++), just enough to read Tiled's exported maps.
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> members;

    bool isNull() const { return type == Type::Null; }
    bool isArray() const { return type == Type::Array; }
    bool has(const std::string& k) const {
        for (const auto& m : members) if (m.first == k) return true;
        return false;
    }
    const Json& operator[](const std::string& k) const {
        for (const auto& m : members) if (m.first == k) return m.second;
        return nullValue();
    }
    const Json& operator[](size_t i) const { return i < arr.size() ? arr[i] : nullValue(); }
    size_t size() const { return type == Type::Array ? arr.size() : members.size(); }
    double number(double d = 0) const { return type == Type::Number ? num : d; }
    std::string string(const std::string& d = "") const { return type == Type::String ? str : d; }
    bool boolean(bool d = false) const { return type == Type::Bool ? b : d; }

    static Json parse(const std::string& text) {
        Parser p{text, 0};
        Json v = p.value();
        p.ws();
        if (p.i != text.size()) p.fail("trailing characters");
        return v;
    }

private:
    static const Json& nullValue() { static const Json n; return n; }

    struct Parser {
        const std::string& s;
        size_t i;
        [[noreturn]] void fail(const std::string& m) const {
            throw std::runtime_error("JSON error at char " + std::to_string(i) + ": " + m);
        }
        char peek() const { return i < s.size() ? s[i] : '\0'; }
        void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
        void expect(const char* w) {
            for (const char* p = w; *p; ++p, ++i)
                if (i >= s.size() || s[i] != *p) fail("bad literal");
        }
        Json value() {
            ws();
            char c = peek();
            if (i >= s.size()) fail("unexpected end of input");
            if (c == '{') return object();
            if (c == '[') return array();
            Json j;
            if (c == '"') { j.type = Type::String; j.str = string(); return j; }
            if (c == 't') { expect("true"); j.type = Type::Bool; j.b = true; return j; }
            if (c == 'f') { expect("false"); j.type = Type::Bool; j.b = false; return j; }
            if (c == 'n') { expect("null"); return j; }
            return number();
        }
        Json number() {
            size_t start = i;
            while (i < s.size()) {
                char c = s[i];
                if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') ++i; else break;
            }
            if (start == i) fail("unexpected character");
            Json j; j.type = Type::Number;
            j.num = std::strtod(s.substr(start, i - start).c_str(), nullptr);
            return j;
        }
        static void appendUtf8(std::string& out, unsigned cp) {
            if (cp < 0x80) out += static_cast<char>(cp);
            else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
            else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        }
        std::string string() {
            ++i;  // opening quote
            std::string out;
            while (true) {
                if (i >= s.size()) fail("unterminated string");
                char c = s[i++];
                if (c == '"') break;
                if (c != '\\') { out += c; continue; }
                if (i >= s.size()) fail("bad escape");
                char e = s[i++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (i + 4 > s.size()) fail("bad \\u escape");
                        unsigned cp = static_cast<unsigned>(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
                        i += 4;
                        appendUtf8(out, cp);
                        break;
                    }
                    default: out += e;  // \" \\ \/
                }
            }
            return out;
        }
        Json array() {
            ++i;
            Json j; j.type = Type::Array;
            ws();
            if (peek() == ']') { ++i; return j; }
            while (true) {
                j.arr.push_back(value());
                ws();
                if (peek() == ',') { ++i; continue; }
                if (peek() == ']') { ++i; break; }
                fail("expected ',' or ']'");
            }
            return j;
        }
        Json object() {
            ++i;
            Json j; j.type = Type::Object;
            ws();
            if (peek() == '}') { ++i; return j; }
            while (true) {
                ws();
                if (peek() != '"') fail("expected string key");
                std::string key = string();
                ws();
                if (peek() != ':') fail("expected ':'");
                ++i;
                j.members.emplace_back(key, value());
                ws();
                if (peek() == ',') { ++i; continue; }
                if (peek() == '}') { ++i; break; }
                fail("expected ',' or '}'");
            }
            return j;
        }
    };
};
