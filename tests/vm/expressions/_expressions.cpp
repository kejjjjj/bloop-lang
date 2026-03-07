#include "tests/vm/defs.hpp"

#define PREFIX "expressions"

TEST_CASE("sequence expressions") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "sequence"), Value::Type::t_int, std::array{		
        (static_cast<void>(2 + 3), static_cast<void>(2), 4),
		(static_cast<void>(2 + 3), 4),
		2 + (static_cast<void>(2 + 3), 4),
		2 + (static_cast<void>(2 + 3), 4) + 3
    });
}
TEST_CASE("integer literals") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "integer_literal"), Value::Type::t_int, std::array{1000000, -500, 1000 - 500});
}
TEST_CASE("integer negation expression") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "negation_int"), Value::Type::t_int, std::array{-5, -10, -15});
}
TEST_CASE("double negation expression") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "negation_double"), Value::Type::t_double, std::array{ -2.5, -1.0, -3.5 });
}
TEST_CASE("prefix increment returns new value") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "prefix_increment"), Value::Type::t_int, std::array{6, 7, 7});
}
TEST_CASE("prefix decrement returns new value") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "prefix_decrement"), Value::Type::t_int, std::array{4, 3, 3});
}
TEST_CASE("postfix increment returns old value") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "postfix_increment"), Value::Type::t_int, std::array{5, 6, 7});
}
TEST_CASE("postfix decrement returns old value") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "postfix_decrement"), Value::Type::t_int, std::array{5, 4, 3});
}
TEST_CASE("subscript get expression") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "subscript"), Value::Type::t_int, std::array{10, 30, 50});
}
TEST_CASE("subscript set expression") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "subscript_set"), Value::Type::t_int, std::array{10, 20, 30});
}
TEST_CASE("property access get expression") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "property_access"), Value::Type::t_int, std::array{1, 2, 3});
}
TEST_CASE("property access set expression") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "property_set"), Value::Type::t_int, std::array{10, 20, 30});
}
TEST_CASE("function expression (lambda)") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "function_expression"), Value::Type::t_int, std::array{7, 25, 42});
}
TEST_CASE("operator precedence") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "operator_precedence"), Value::Type::t_int, std::array{14, 20, 5, 26, 3, 5});
}