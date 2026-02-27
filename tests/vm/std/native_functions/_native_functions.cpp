#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("std", "native_functions")

TEST_CASE("std length") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "length"), Value::Type::t_int, std::array{3, 0, 6, 13, 0});
}