#include "tests/vm/defs.hpp"

#include <iostream>
#include <sstream>

#define PREFIX JOIN_PATH("control_flow", "ranged_for")

TEST_CASE("ranged for | sum of an array") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "sum_array"), Value::Type::t_int, 6);
}
TEST_CASE("ranged for | in a function, also with an empty array") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "in_function"), Value::Type::t_int, std::array{ 10, 0, 10 });
}
TEST_CASE("ranged for | elements are copies, objects are shared") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "copies"), Value::Type::t_int, std::array{ 1, 3, 10, 20 });
}
TEST_CASE("ranged for | strings iterate their characters") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "string"), Value::Type::t_object, BLOOPTEXT("cba"));
}
TEST_CASE("ranged for | break and continue") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "break_continue"), Value::Type::t_int, 8);
}
TEST_CASE("ranged for | nested loops") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "nested"), Value::Type::t_int, 140);
}
TEST_CASE("ranged for | returning from inside the loop") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "return_from_loop"), Value::Type::t_object, BLOOPTEXT("found|missing"));
}
TEST_CASE("ranged for | closures capture the element of their own iteration") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "closures"), Value::Type::t_int, std::array{ 30, 60, 90 });
}
TEST_CASE("ranged for | elements are read on every iteration") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "modify_array"), Value::Type::t_int, std::array{ 1, 2, 100 });
}
TEST_CASE("ranged for | the iterable survives garbage collections") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "gc_during_loop"), Value::Type::t_object, BLOOPTEXT("a1b2c3"));
}
TEST_CASE("ranged for | iterating a non-iterable value is a runtime error") {
    const auto ret = bloop::test::TEST_ExecuteFile(MAKE_RELATIVE_PATH(PREFIX, "not_iterable"));
    const auto& output = bloop::test::TEST_LastOutput();

    REQUIRE(ret.has_value());
    REQUIRE(ret->type == Value::Type::t_undefined);
    REQUIRE(output.find("a value of type \"int\" is not iterable") != std::string::npos);
}
TEST_CASE("ranged for | a const element can't be assigned") {
    REQUIRE_THROWS(bloop::test::TEST_CompileFile(MAKE_RELATIVE_PATH(PREFIX, "assign_const")));
}
