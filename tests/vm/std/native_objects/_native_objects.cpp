#include "tests/vm/defs.hpp"

#include <iostream>
#include <sstream>

#define PREFIX JOIN_PATH("std", "native_objects")

TEST_CASE("std console log and property") {

    // capture what console.log prints
    const auto ret = bloop::test::TEST_ExecuteFile(MAKE_RELATIVE_PATH(PREFIX, "console"));
    const auto& output = bloop::test::TEST_LastOutput();

    REQUIRE(output.find("hello 42 world\n") != std::string::npos);

    REQUIRE(ret.has_value());
    REQUIRE(ret->IsArray());
    REQUIRE(ret->obj->array.values[0].type == Value::Type::t_int);
    REQUIRE(ret->obj->array.values[0].i == 1); // used to read 8 bytes out of a 1 byte string
    REQUIRE(ret->obj->array.values[1].type == Value::Type::t_bool);
    REQUIRE(ret->obj->array.values[1].b);
}
