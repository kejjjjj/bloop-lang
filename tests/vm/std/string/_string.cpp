#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("std.string.substring") {
    Expect("return std.string.substring(\"hello\", 1, 3);", "el");
    Expect("return std.string.substring(\"hello\", 2);", "llo");
    Expect("return std.string.substring(\"hello\", -3);", "llo");
    Expect("return std.string.substring(\"hello\", 0, -1);", "hell");
    Expect("return std.length(std.string.substring(\"hello\", 4, 2));", "0");
    Expect("return std.string.substring(\"\", 0, 10);", "");
}
TEST_CASE("std.string.find_last") {
    Expect("return [std.string.find_last(\"banana\", \"an\"), std.string.find_last(\"banana\", \"an\", 2), std.string.find_last(\"banana\", \"x\")];", "[ 3, 1, -1 ]");
    Expect("return [std.string.find_last(\"abc\", \"a\", 0), std.string.find_last(\"abc\", \"a\", -1), std.string.find_last(\"abc\", \"\")];", "[ 0, -1, 3 ]");
}
TEST_CASE("std.string.find, contains, starts_with, ends_with") {
    Expect("return [std.string.find(\"banana\", \"an\"), std.string.find(\"banana\", \"an\", 2), std.string.find(\"banana\", \"x\")];", "[ 1, 3, -1 ]");
    Expect("return [std.string.find(\"abc\", \"\"), std.string.find(\"abc\", \"c\", 10)];", "[ 0, -1 ]");
    Expect("return [std.string.contains(\"hello\", \"ell\"), std.string.contains(\"hello\", \"elo\")];", "[ true, false ]");
    Expect("return [std.string.starts_with(\"hello\", \"he\"), std.string.starts_with(\"hello\", \"lo\")];", "[ true, false ]");
    Expect("return [std.string.ends_with(\"hello\", \"lo\"), std.string.ends_with(\"hello\", \"he\")];", "[ true, false ]");
}
TEST_CASE("std.string.split") {
    Expect("return std.string.split(\"a,b,,c\", \",\");", "[ a, b, , c ]");
    Expect("return std.string.split(\"a--b\", \"--\");", "[ a, b ]");
    Expect("return std.string.split(\"abc\", \"\");", "[ a, b, c ]");
    Expect("return std.string.split(\"abc\", \";\");", "[ abc ]");
    Expect("return std.length(std.string.split(\"\", \",\"));", "1"); // like js: [""]
}
TEST_CASE("std.string.replace and clone") {
    Expect("return std.string.replace(\"a-b-c\", \"-\", \"+\");", "a+b+c");
    Expect("return std.string.replace(\"aaa\", \"aa\", \"b\");", "ba");
    Expect("return std.string.replace(\"abc\", \"x\", \"y\");", "abc");
    ExpectError("return std.string.replace(\"abc\", \"\", \"y\");", "can't replace an empty string");
    Expect("return std.string.clone(\"ab\", 3);", "ababab");
    Expect("return std.length(std.string.clone(\"ab\", 0));", "0");
    ExpectError("return std.string.clone(\"ab\", -1);", "can't repeat -1 times");
}
TEST_CASE("std.string.to_upper, lower and trim") {
    Expect("return std.string.to_upper(\"Hello, World 1!\");", "HELLO, WORLD 1!");
    Expect("return std.string.to_lower(\"Hello, World 1!\");", "hello, world 1!");
    Expect("return \"[\" + std.string.trim(\"  \\t padded \\n \") + \"]\";", "[padded]");
}
TEST_CASE("std.string.get_code_at and from_char_code") {
    Expect("return [std.string.get_code_at(\"AZ\", 0), std.string.get_code_at(\"AZ\", 1)];", "[ 65, 90 ]");
    Expect("return std.string.from_char_code(104) + std.string.from_char_code(105);", "hi");
    ExpectError("return std.string.get_code_at(\"A\", 1);", "index 1 is out of range for a string of length 1");
    ExpectError("return std.string.from_char_code(256);", "256 is not in the range 0 - 255");
}
TEST_CASE("std.string.from") {
    Expect("return std.string.from(42) + std.string.from(true) + std.string.from(undefined);", "42trueundefined");
    Expect("return std.string.from([1, \"a\"]);", "[ 1, a ]");
    Expect("return \"n = \" + std.string.from(-7);", "n = -7");
}
TEST_CASE("std.string.parse_int and parse_float") {
    Expect("return [std.string.parse_int(\"42\"), std.string.parse_int(\" -17 \"), std.string.parse_int(\"+5\")];", "[ 42, -17, 5 ]");
    Expect("return std.string.parse_int(\"9000000000\");", "9000000000"); // 64-bit
    // anything that isn't a whole integer is undefined
    Expect("return [std.string.parse_int(\"\"), std.string.parse_int(\"12abc\"), std.string.parse_int(\"1.5\"), std.string.parse_int(\"99999999999999999999\")];",
        "[ undefined, undefined, undefined, undefined ]");
    Expect("return [std.string.parse_float(\"1.5\"), std.string.parse_float(\"-2\"), std.string.parse_float(\" 1e3 \")];", "[ 1.500000, -2.000000, 1000.000000 ]");
    Expect("return [std.string.parse_float(\"abc\"), std.string.parse_float(\"1.5x\"), std.string.parse_float(\"\")];", "[ undefined, undefined, undefined ]");

    const auto result = RunScript("return std.string.parse_int(\"7\");");
    REQUIRE(result.value->type == Value::Type::t_int);
}
TEST_CASE("std.string functions only accept strings") {
    ExpectError("return std.string.to_upper(5);", "argument 1 must be \"string\", but got \"int\"");
    ExpectError("return std.string.find(\"abc\", [\"a\"]);", "argument 2 must be \"string\", but got \"array\"");
}
