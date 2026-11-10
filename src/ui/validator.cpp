#include "ui/validator.h"

#include <istream>
#include <ostream>

#include "common/messages.h"

namespace validator {
namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

enum class Digits { Ok, NotNumeric, TooBig };

Digits readDigits(const std::string& s, std::size_t from, long long& value) {
    value = 0;
    bool tooBig = false;
    for (std::size_t i = from; i < s.size(); ++i) {
        const char c = s[i];
        if (c < '0' || c > '9') return Digits::NotNumeric;
        if (!tooBig) {
            value = value * 10 + (c - '0');
            tooBig = value > 1000000000000LL;              // far beyond any valid field
        }
    }
    return tooBig ? Digits::TooBig : Digits::Ok;
}

// Optional sign followed by digits.
Digits readSigned(const std::string& s, long long& value) {
    std::size_t from = 0;
    bool negative = false;
    if (s[0] == '-' || s[0] == '+') {
        negative = (s[0] == '-');
        from = 1;
    }
    if (from >= s.size()) return Digits::NotNumeric;
    const Digits d = readDigits(s, from, value);
    if (d == Digits::Ok && negative) value = -value;
    return d;
}

}  // namespace

Result<std::string> checkText(const std::string& raw, std::size_t minLen, std::size_t maxLen,
                              bool trimInput) {
    if (raw.size() > kMaxInputLength) return Result<std::string>::failure(ErrorCode::BadLength);
    std::string t = trimInput ? trim(raw) : raw;
    if (t.size() < minLen || t.size() > maxLen) {
        return Result<std::string>::failure(ErrorCode::BadLength);
    }
    return Result<std::string>::success(t);
}

Result<long long> parseInteger(const std::string& raw, long long lo, long long hi) {
    if (raw.size() > kMaxInputLength) return Result<long long>::failure(ErrorCode::BadLength);
    const std::string s = trim(raw);
    if (s.empty()) return Result<long long>::failure(ErrorCode::BadLength);

    long long value = 0;
    const Digits d = readSigned(s, value);
    if (d == Digits::NotNumeric) return Result<long long>::failure(ErrorCode::NotNumeric);
    if (d == Digits::TooBig) return Result<long long>::failure(ErrorCode::OutOfRange);
    if (value < lo || value > hi) return Result<long long>::failure(ErrorCode::OutOfRange);
    return Result<long long>::success(value);
}

bool readLine(std::istream& in, std::string& out) {
    if (!std::getline(in, out)) return false;
    if (!out.empty() && out.back() == '\r') out.pop_back();
    return true;
}

Result<long long> readInteger(std::istream& in, std::ostream& out, const std::string& field,
                              long long lo, long long hi, int maxAttempts) {
    Result<long long> last = Result<long long>::failure(ErrorCode::BadLength);
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        out << field << " (" << lo << "-" << hi << "): ";
        std::string line;
        if (!readLine(in, line)) return Result<long long>::failure(ErrorCode::BadLength);
        last = parseInteger(line, lo, hi);
        if (last.ok()) return last;
        out << errorMessage(last.error, field, lo, hi) << "\n";
    }
    return last;
}

Result<std::string> readText(std::istream& in, std::ostream& out, const std::string& field,
                             std::size_t minLen, std::size_t maxLen, int maxAttempts) {
    Result<std::string> last = Result<std::string>::failure(ErrorCode::BadLength);
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        out << field << ": ";
        std::string line;
        if (!readLine(in, line)) return Result<std::string>::failure(ErrorCode::BadLength);
        last = checkText(line, minLen, maxLen);
        if (last.ok()) return last;
        out << errorMessage(last.error, field) << "\n";
    }
    return last;
}

}  // namespace validator
