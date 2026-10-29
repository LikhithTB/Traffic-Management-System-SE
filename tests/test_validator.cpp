#include <sstream>
#include "test_framework.h"
#include "ui/validator.h"

using namespace validator;

TEST(validator_accepts_valid_integers) {
    CHECK_EQ(*parseInteger("42", 5, 120).value, 42);
    CHECK_EQ(*parseInteger("  7  ", 5, 120).value, 7);
    CHECK_EQ(*parseInteger("5", 5, 120).value, 5);        // boundaries
    CHECK_EQ(*parseInteger("120", 5, 120).value, 120);
    CHECK_EQ(*parseInteger("-3", -10, 10).value, -3);
}

TEST(validator_rejects_non_numeric) {
    CHECK_EQ(parseInteger("abc", 5, 120).error, ErrorCode::NotNumeric);
    CHECK_EQ(parseInteger("4x", 5, 120).error, ErrorCode::NotNumeric);
    CHECK_EQ(parseInteger("1.5", 5, 120).error, ErrorCode::NotNumeric);
    CHECK_EQ(parseInteger("-", 5, 120).error, ErrorCode::NotNumeric);
}

TEST(validator_rejects_out_of_range) {
    CHECK_EQ(parseInteger("4", 5, 120).error, ErrorCode::OutOfRange);
    CHECK_EQ(parseInteger("121", 5, 120).error, ErrorCode::OutOfRange);
    CHECK_EQ(parseInteger("99999999999999999999999", 5, 120).error, ErrorCode::OutOfRange);
}

TEST(validator_rejects_empty_and_too_long) {
    CHECK_EQ(parseInteger("", 5, 120).error, ErrorCode::BadLength);
    CHECK_EQ(parseInteger("   ", 5, 120).error, ErrorCode::BadLength);
    CHECK_EQ(parseInteger(std::string(257, '1'), 5, 120).error, ErrorCode::BadLength);
    CHECK_EQ(checkText("", 1, 20).error, ErrorCode::BadLength);
    CHECK_EQ(checkText(std::string(257, 'a')).error, ErrorCode::BadLength);
    CHECK_EQ(checkText("abc", 4, 20).error, ErrorCode::BadLength);   // username too short
    CHECK(checkText(std::string(256, 'a')).ok());                    // exactly 256 allowed
}

TEST(validator_text_trim_option) {
    CHECK_EQ(*checkText("  bob  ").value, "bob");
    CHECK_EQ(*checkText("  pw  ", 1, 256, false).value, "  pw  ");   // passwords keep spaces
}

TEST(validator_read_integer_names_field_and_range_then_retries) {
    std::istringstream in("abc\n200\n30\n");
    std::ostringstream out;
    auto r = readInteger(in, out, "Green time", 5, 120);
    CHECK(r.ok());
    CHECK_EQ(*r.value, 30);
    const std::string text = out.str();
    CHECK(text.find("Green time must be a number") != std::string::npos);
    CHECK(text.find("Green time must be between 5 and 120") != std::string::npos);
}

TEST(validator_read_integer_gives_up_after_max_attempts) {
    std::istringstream in("x\ny\nz\n30\n");
    std::ostringstream out;
    auto r = readInteger(in, out, "Lanes", 1, 8, 3);
    CHECK(!r.ok());
    CHECK_EQ(r.error, ErrorCode::NotNumeric);
}

TEST(validator_read_handles_end_of_input_and_crlf) {
    std::istringstream empty("");
    std::ostringstream out;
    CHECK(!readInteger(empty, out, "Lanes", 1, 8).ok());
    std::istringstream crlf("6\r\n");
    CHECK_EQ(*readInteger(crlf, out, "Lanes", 1, 8).value, 6);
}
