#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "equality")

TEST_CASE("equality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "equality"), Value::Type::t_bool, std::array{
        bloop::BloopBool(true),   // 1 == 1
        bloop::BloopBool(true),   // 1 == true
        bloop::BloopBool(true),   // 1 == 1.0
        bloop::BloopBool(true),   // 1 == 1u
        bloop::BloopBool(false),  // 1 == 2
        bloop::BloopBool(false),  // 1 == false
        bloop::BloopBool(false),  // 1 == 1.1
        bloop::BloopBool(true),   // "hello" == "hello"
        bloop::BloopBool(false),  // "hello1" == "hello"
        bloop::BloopBool(true),   // undefined == undefined
        bloop::BloopBool(true),   // main == main
        bloop::BloopBool(false),  // (() => 1) == (() => 1)
        bloop::BloopBool(false),  // [] == []
        bloop::BloopBool(false),  // {} == {}
    });
}
TEST_CASE("unequality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "unequality"), Value::Type::t_bool, std::array{
        bloop::BloopBool(false),  // 1 != 1
        bloop::BloopBool(false),  // 1 != true
        bloop::BloopBool(false),  // 1 != 1.0
        bloop::BloopBool(false),  // 1 != 1u
        bloop::BloopBool(true),   // 1 != 2
        bloop::BloopBool(true),   // 1 != false
        bloop::BloopBool(true),   // 1 != 1.1
        bloop::BloopBool(false),  // "hello" != "hello"
        bloop::BloopBool(true),   // "hello1" != "hello"
        bloop::BloopBool(false),  // undefined != undefined
        bloop::BloopBool(false),  // main != main
        bloop::BloopBool(true),   // (() => 1) != (() => 1)
        bloop::BloopBool(true),   // [] != []
        bloop::BloopBool(true),   // {} != {}
    });
}
TEST_CASE("strict equality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "strict_equality"), Value::Type::t_bool, std::array{
        bloop::BloopBool(true),   // 1 === 1
        bloop::BloopBool(false),  // 1 === true
        bloop::BloopBool(false),  // 1 === 1.0
        bloop::BloopBool(false),  // 1 === 1u
        bloop::BloopBool(false),  // 1 === 2
        bloop::BloopBool(false),  // 1 === false
        bloop::BloopBool(true),   // "hello" === "hello"
        bloop::BloopBool(false),  // "hello1" === "hello"
        bloop::BloopBool(true),   // undefined === undefined
        bloop::BloopBool(true),   // main === main
        bloop::BloopBool(false),  // (() => 1) === (() => 1)
        bloop::BloopBool(false),  // [] === []
        bloop::BloopBool(false),  // {} === {}
    });
}
TEST_CASE("strict unequality operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "strict_unequality"), Value::Type::t_bool, std::array{
        bloop::BloopBool(false),  // 1 !== 1
        bloop::BloopBool(true),   // 1 !== true
        bloop::BloopBool(true),   // 1 !== 1.0
        bloop::BloopBool(true),   // 1 !== 1u
        bloop::BloopBool(true),   // 1 !== 2
        bloop::BloopBool(true),   // 1 !== false
        bloop::BloopBool(false),  // "hello" !== "hello"
        bloop::BloopBool(true),   // "hello1" !== "hello"
        bloop::BloopBool(false),  // undefined !== undefined
        bloop::BloopBool(false),  // main !== main
        bloop::BloopBool(true),   // (() => 1) !== (() => 1)
        bloop::BloopBool(true),   // [] !== []
        bloop::BloopBool(true),   // {} !== {}
    });
}
