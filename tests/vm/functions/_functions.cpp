#include "tests/vm/defs.hpp"

#define PREFIX "functions"

TEST_CASE("main function calls a function with the value 50 and returns it") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "function_returns_argument"), Value::Type::t_int, 50);
}
TEST_CASE("main function calls a function that edits a global variable to 50") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "function_edits_global_variable"), Value::Type::t_int, 50);
}
TEST_CASE("main function calls a function that doesn't return") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "function_no_return"), Value::Type::t_undefined, bloop::test::emptyStruct);
}
TEST_CASE("main function calls a function that recursively calls itself until v > 5") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "simple_recursion"), Value::Type::t_int, 6);
}
TEST_CASE("main function calls a function with the value [50, 1] and returns it") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "function_returns_shared_argument"), Value::Type::t_int, std::array{ 50, 1 });
}