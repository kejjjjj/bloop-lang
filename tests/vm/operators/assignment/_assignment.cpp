#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "assignment")

TEST_CASE("assignment operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "script"), Value::Type::t_int, std::array{		
		bloop::BloopInt(10) + bloop::BloopInt(10),
		bloop::BloopInt(10) - bloop::BloopInt(5),
		bloop::BloopInt(10) * bloop::BloopInt(2),
		bloop::BloopInt(10) / bloop::BloopInt(2),
		bloop::BloopInt(10) % bloop::BloopInt(2),
		bloop::BloopInt(10) << bloop::BloopInt(2),
		bloop::BloopInt(10) >> bloop::BloopInt(2),
		bloop::BloopInt(10) | bloop::BloopInt(2),
		bloop::BloopInt(10) ^ bloop::BloopInt(2),
		bloop::BloopInt(10) & bloop::BloopInt(2)
    });
}
TEST_CASE("chained assignments are right associative") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "chained"), Value::Type::t_int, std::array{
        2, 5, 5,    // a = b = c = 5, then a = 2
        7, 7, 7,    // arr[0] = obj.x = arr[1] = 7
        2, 2,       // d += a = 2; e = d -= 1
        9, 9,       // captured = local = 9
        20 - 5 - 3  // the other operators are still left associative
    });
}
TEST_CASE("chained assignment to a const is still an error") {
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileFile(MAKE_RELATIVE_PATH(PREFIX, "chained_const")), Catch::Matchers::ContainsSubstring("const"));
}
TEST_CASE("assignments to elements and properties produce the assigned value") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "assignment_value"), Value::Type::t_bool, std::array{ true, true, true, true, true });
}
