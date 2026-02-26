#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "additive")

TEST_CASE("integer additive operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "int"), Value::Type::t_int, std::array{		
		bloop::BloopInt(1) + bloop::BloopInt(2),
		bloop::BloopInt(5) + bloop::BloopInt(2),
		bloop::BloopInt(1001) + bloop::BloopInt(50),
		bloop::BloopInt(-200) + bloop::BloopInt(5),
    });
}
TEST_CASE("uinteger additive operations") {
    
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "uint"), Value::Type::t_uint, std::array{		
		bloop::BloopUInt(1u) + bloop::BloopUInt(2u),
		bloop::BloopUInt(5u) + bloop::BloopUInt(2u),
		bloop::BloopUInt(1001u) + bloop::BloopUInt(50u),
		static_cast<bloop::BloopUInt>(-1)
    });
}
TEST_CASE("double additive operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "double"), Value::Type::t_double, std::array{		
		1.2 + 2.6,
		5.1 + 2.74,
		1001.1234 + 50.1234,
		-200.2 + 5,
    });
}
TEST_CASE("string additive operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "string"), Value::Type::t_object, std::array{		
		BLOOPTEXT("hello world"),
		BLOOPTEXT("\"h\"\"e\""),
		// #ifdef _WIN32
		// BLOOPTEXT("hello\r\n\t\tworld"),
		// #else
		// BLOOPTEXT("hello\n\t\tworld"),
		// #endif
		BLOOPTEXT("\'hello, world\'"),
		BLOOPTEXT("\"hello, world\"")
    });
}