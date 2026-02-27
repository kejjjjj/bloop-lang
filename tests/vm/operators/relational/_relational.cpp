#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "relational")

TEST_CASE("integer relational operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "int"), Value::Type::t_bool, std::array{
        // less than
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // less than or equal
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // greater than or equal
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // greater than
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
    });
}
TEST_CASE("uinteger relational operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "uint"), Value::Type::t_bool, std::array{
        // less than
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // less than or equal
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // greater than or equal
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // greater than
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
    });
}
TEST_CASE("double relational operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "double"), Value::Type::t_bool, std::array{
        // less than
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // less than or equal
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // greater than or equal
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // greater than
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
    });
}
TEST_CASE("undefined relational operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "undefined"), Value::Type::t_bool, std::array{
        bloop::BloopBool(true),   // undefined < undefined
        bloop::BloopBool(true),   // undefined <= undefined
        bloop::BloopBool(true),   // undefined >= undefined
        bloop::BloopBool(true),   // undefined > undefined
    });
}
