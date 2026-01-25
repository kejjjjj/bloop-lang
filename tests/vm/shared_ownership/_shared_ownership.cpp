#include "tests/vm/defs.hpp"

#define PREFIX "shared_ownership"

TEST_CASE("array [0, 1] gets copied and then the copy gets a value 50 assigned to [0]") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "array"), Value::Type::t_int, std::array{50, 1});
}
TEST_CASE("object {a: 0, b: 1} gets copied and then the copy gets a value 50 assigned to a") {
    bloop::test::CheckObject(MAKE_RELATIVE_PATH(PREFIX, "object"), Value::Type::t_int, std::array{ 
        std::make_pair(bloop::BloopString("a"), 50), 
        std::make_pair(bloop::BloopString("b"), 1)
    });
}