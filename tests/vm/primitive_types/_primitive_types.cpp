#include "tests/vm/defs.hpp"

#define PREFIX "primitive_types"


TEST_CASE("undefined constant") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "undefined"), Value::Type::t_undefined, bloop::test::emptyStruct);
}
TEST_CASE("boolean constant returns true") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "boolean"), Value::Type::t_bool, true);
}
TEST_CASE("integer constant returns 64") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "integer"), Value::Type::t_int, 64);
}
TEST_CASE("unsigned integer constant returns 64u") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "uinteger"), Value::Type::t_uint, 64u);
}
TEST_CASE("double constant returns 64.0") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "double"), Value::Type::t_double, 64.0);
}
TEST_CASE("string constant returns Hello, world!") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "string"), Value::Type::t_object, BLOOPTEXT("Hello, World!"));
}