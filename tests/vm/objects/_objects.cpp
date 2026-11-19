#include "tests/vm/defs.hpp"

#define PREFIX "objects"

TEST_CASE("objects | adding properties to a full object") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "add_property_to_full_object"), Value::Type::t_int, std::array{ 1, 2, 3, 4, 5, 6 });
}
TEST_CASE("objects | growing an empty object to 20 properties") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "grow_from_empty"), Value::Type::t_int, std::array{ 100, 2, 7, 13, 19, 2000 });
}
TEST_CASE("objects | a missing property on a full object is an error, not a hang") {
    // the error is expected: the lookup used to loop forever
    const auto ret = bloop::test::TEST_ExecuteFile(MAKE_RELATIVE_PATH(PREFIX, "missing_property_on_full_object"));
    REQUIRE(ret.has_value());
    REQUIRE(ret->type == Value::Type::t_undefined); // runtime errors make Run() return undefined
    REQUIRE(bloop::test::TEST_LastOutput().find("unknown property \"missing\"") != std::string::npos);
}
TEST_CASE("objects | references see the properties added by growing") {
    const auto ret = bloop::test::TEST_ExecuteFile(MAKE_RELATIVE_PATH(PREFIX, "shared_after_growth"));
    REQUIRE(ret.has_value());
    REQUIRE(ret->IsArray());

    const auto* values = ret->obj->array.values;
    REQUIRE(values[0].type == Value::Type::t_int);
    REQUIRE(values[0].i == 7);
    REQUIRE(values[1].i == 6);
    REQUIRE(values[2].type == Value::Type::t_bool);
    REQUIRE(values[2].b);
    REQUIRE(values[3].b);
}
