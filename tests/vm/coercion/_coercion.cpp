#include "tests/vm/defs.hpp"

#define PREFIX "coercion"

TEST_CASE("boolean coerced to int") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "boolean_to_int"), Value::Type::t_int, 2);
}
TEST_CASE("boolean coerced to uint") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "boolean_to_uint"), Value::Type::t_uint, 2u);
}
TEST_CASE("boolean coerced to double") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "boolean_to_double"), Value::Type::t_double, 3.0);
}
TEST_CASE("int coerced to double") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "int_to_double"), Value::Type::t_double, 3.0);
}
TEST_CASE("uint coerced to int") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "uint_to_int"), Value::Type::t_int, 2);
}
TEST_CASE("uint coerced to double") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "uint_to_double"), Value::Type::t_double, 3.0);
}