#include "tests/vm/defs.hpp"

#define PREFIX "integers"

TEST_CASE("integers are 64-bit everywhere") {
    static_assert(sizeof(bloop::BloopInt) == 8 && sizeof(bloop::BloopUInt) == 8);
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "beyond_32_bits"), Value::Type::t_int,
        std::array<bloop::BloopInt, 4>{ 2147483648ll, 10000000000ll, -2147483649ll, 4294967296ll });
}
