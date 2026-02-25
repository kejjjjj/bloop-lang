#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("control_flow", "if")

TEST_CASE("if statement is true") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "if_statement_is_true"), Value::Type::t_bool, true);
}
TEST_CASE("else if statement is true") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "else_if_statement_is_true"), Value::Type::t_bool, true);
}
TEST_CASE("else statement is true") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "else_statement"), Value::Type::t_bool, true);
}