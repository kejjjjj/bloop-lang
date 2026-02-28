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