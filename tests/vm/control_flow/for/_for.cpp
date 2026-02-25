#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("control_flow", "for")

TEST_CASE("for statement is 5") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "for_statement_is_5"), Value::Type::t_int, 5);
}
TEST_CASE("for statement less than 5 breaks at 2") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "for_less_than_5_breaks_at_2"), Value::Type::t_int, 2);
}
TEST_CASE("for statement less than 5 adds odd numbers to array") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "for_odd_numbers"), Value::Type::t_int, std::array{1, 3, 5});
}