#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "shift")

TEST_CASE("integer shift operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "int"), Value::Type::t_int, std::array{
        // left shift
        bloop::BloopInt(1) << bloop::BloopInt(4),
        bloop::BloopInt(16) << bloop::BloopInt(2),
        // right shift
        bloop::BloopInt(64) >> bloop::BloopInt(2),
        bloop::BloopInt(256) >> bloop::BloopInt(4),
    });
}
TEST_CASE("uinteger shift operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "uint"), Value::Type::t_uint, std::array{
        // left shift
        bloop::BloopUInt(1u) << bloop::BloopUInt(4u),
        bloop::BloopUInt(16u) << bloop::BloopUInt(2u),
        // right shift
        bloop::BloopUInt(64u) >> bloop::BloopUInt(2u),
        bloop::BloopUInt(256u) >> bloop::BloopUInt(4u),
    });
}
