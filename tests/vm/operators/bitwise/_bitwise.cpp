#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "bitwise")

TEST_CASE("integer bitwise operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "int"), Value::Type::t_int, std::array{		
		bloop::BloopInt(16) & bloop::BloopInt(4),
		bloop::BloopInt(20) & bloop::BloopInt(4),
		bloop::BloopInt(16) | bloop::BloopInt(32),
		bloop::BloopInt(512) | bloop::BloopInt(5),
		bloop::BloopInt(12) ^ bloop::BloopInt(6),
		bloop::BloopInt(5) ^ bloop::BloopInt(2),
    });
}
TEST_CASE("uinteger bitwise operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "uint"), Value::Type::t_uint, std::array{		
		bloop::BloopUInt(16) & bloop::BloopUInt(4),
		bloop::BloopUInt(20) & bloop::BloopUInt(4),
		bloop::BloopUInt(16) | bloop::BloopUInt(32),
		bloop::BloopUInt(512) | bloop::BloopUInt(5),
		bloop::BloopUInt(12) ^ bloop::BloopUInt(6),
		bloop::BloopUInt(5) ^ bloop::BloopUInt(2),
    });
}