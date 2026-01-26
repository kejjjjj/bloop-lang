#include "tests/vm/defs.hpp"

#define PREFIX "composite_types"

TEST_CASE("function call that returns 50") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "function_call"), Value::Type::t_int, 50);
}
TEST_CASE("array that returns [0, 1, 2]") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "array"), Value::Type::t_int, std::array{0,1,2});
}
TEST_CASE("object that returns { a: 1, b: 2, c: 3 }") {
    bloop::test::CheckObject(MAKE_RELATIVE_PATH(PREFIX, "object"), Value::Type::t_int, std::array{
        std::make_pair(bloop::BloopString("a"), 1),
        std::make_pair(bloop::BloopString("b"), 2),
        std::make_pair(bloop::BloopString("c"), 3)
    });
}