#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "equality")

TEST_CASE("integer equality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "int"), Value::Type::t_bool, std::array{
        // equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // not equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
    });
}
TEST_CASE("uinteger equality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "uint"), Value::Type::t_bool, std::array{
        // equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // not equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
    });
}
TEST_CASE("double equality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "double"), Value::Type::t_bool, std::array{
        // equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // not equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
    });
}
TEST_CASE("string equality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "string"), Value::Type::t_bool, std::array{
        // equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // not equal
        bloop::BloopBool(true),
        bloop::BloopBool(false),
    });
}
