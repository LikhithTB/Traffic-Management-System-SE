#pragma once
// Menu & Input Validator (story #6, NFR-06, SEC-06).
// Every console input is length-limited (<= 256) and range-checked here before
// any other module sees it. No scanf/gets/strcpy: std::string only.
#include <cstddef>
#include <iosfwd>
#include <string>
#include "common/result.h"

namespace validator {

constexpr std::size_t kMaxInputLength = 256;

// Text field. Rejects empty (after optional trim) or longer than maxLen / 256 -> E-VAL-003.
Result<std::string> checkText(const std::string& raw, std::size_t minLen = 1,
                              std::size_t maxLen = kMaxInputLength, bool trimInput = true);

// Integer in [lo, hi]. Empty/too long -> BadLength, non-digits -> NotNumeric,
// outside range -> OutOfRange.
Result<long long> parseInteger(const std::string& raw, long long lo, long long hi);

// Reads one line (strips trailing \r). Returns false at end of input.
bool readLine(std::istream& in, std::string& out);

// Prompts, reads and validates; on bad input prints a message naming the field
// and the allowed range, then asks again (up to maxAttempts).
Result<long long> readInteger(std::istream& in, std::ostream& out, const std::string& field,
                              long long lo, long long hi, int maxAttempts = 3);
Result<std::string> readText(std::istream& in, std::ostream& out, const std::string& field,
                             std::size_t minLen = 1, std::size_t maxLen = kMaxInputLength,
                             int maxAttempts = 3);

}  // namespace validator
