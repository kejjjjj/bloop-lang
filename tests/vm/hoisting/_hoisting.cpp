#include "tests/vm/defs.hpp"

#define PREFIX "hoisting"

TEST_CASE("call function below main") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "hoisting_function_basic"), Value::Type::t_object, BLOOPTEXT("ok"));
}
TEST_CASE("mutual recursion with correct assignment order") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "hoisting_mutual_functions"), Value::Type::t_int, std::array{1,0,0,1});
}
TEST_CASE("closure calls hoisted function") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "hoisting_valid_closure"), Value::Type::t_object, BLOOPTEXT("f"));
}