#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "logical")

TEST_CASE("logical and operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "logical_and"), Value::Type::t_bool, std::array{
        // bool && bool
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // int && int
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // uint && uint
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // double && double
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
    });
}
TEST_CASE("logical or operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "logical_or"), Value::Type::t_bool, std::array{
        // bool || bool
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // int || int
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // uint || uint
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // double || double
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
    });
}
