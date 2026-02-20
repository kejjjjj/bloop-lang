#include "tests/vm/defs.hpp"

#define PREFIX "expressions"

TEST_CASE("sequence expressions") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "sequence"), Value::Type::t_int, std::array{		
        (static_cast<void>(2 + 3), static_cast<void>(2), 4),
		(static_cast<void>(2 + 3), 4),
		2 + (static_cast<void>(2 + 3), 4),
		2 + (static_cast<void>(2 + 3), 4) + 3
    });
}
TEST_CASE("integer literals") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "integer_literal"), Value::Type::t_int, std::array{1000000, -500, 1000 - 500});
}