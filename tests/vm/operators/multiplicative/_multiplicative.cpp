#include "tests/vm/defs.hpp"

#include <cmath>

#define PREFIX JOIN_PATH("operators", "multiplicative")

TEST_CASE("integer multiplicative operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "int"), Value::Type::t_int, std::array{
        // multiply
        bloop::BloopInt(3) * bloop::BloopInt(4),
        bloop::BloopInt(-2) * bloop::BloopInt(5),
        // divide
        bloop::BloopInt(10) / bloop::BloopInt(2),
        bloop::BloopInt(7) / bloop::BloopInt(2),
        // modulo
        bloop::BloopInt(10) % bloop::BloopInt(3),
        bloop::BloopInt(7) % bloop::BloopInt(4),
    });
}
TEST_CASE("uinteger multiplicative operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "uint"), Value::Type::t_uint, std::array{
        // multiply
        bloop::BloopUInt(3u) * bloop::BloopUInt(4u),
        bloop::BloopUInt(2u) * bloop::BloopUInt(5u),
        // divide
        bloop::BloopUInt(10u) / bloop::BloopUInt(2u),
        bloop::BloopUInt(7u) / bloop::BloopUInt(2u),
        // modulo
        bloop::BloopUInt(10u) % bloop::BloopUInt(3u),
        bloop::BloopUInt(7u) % bloop::BloopUInt(4u),
    });
}
TEST_CASE("double multiplicative operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "double"), Value::Type::t_double, std::array{
        // multiply
        3.0 * 4.0,
        -2.0 * 5.0,
        // divide
        10.0 / 4.0,
        7.0 / 2.0,
        // modulo
        std::fmod(10.0, 3.0),
        std::fmod(7.5, 2.0),
    });
}
