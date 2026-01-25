#include "tests/vm/defs.hpp"

#define PREFIX "exceptions"

TEST_CASE("simple throw within try catch") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "simple_try_catch"), Value::Type::t_object, BLOOPTEXT("test exception"));
}
TEST_CASE("nested try catch") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "nested_try_catch"), Value::Type::t_object, BLOOPTEXT("test exception"));
}
TEST_CASE("triple nested try catch") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "triple_nested_try_catch"), Value::Type::t_object, BLOOPTEXT("exceptionFunc"));
}
TEST_CASE("deeply nested try catch") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "deeply_nested_try_catch"), Value::Type::t_object, BLOOPTEXT("exceptionFunc"));
}
TEST_CASE("try catch doesn't throw") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "try_catch_no_throw"), Value::Type::t_object, BLOOPTEXT("try"));
}
TEST_CASE("try catch returns from try block") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "try_catch_returns_from_try"), Value::Type::t_object, BLOOPTEXT("try"));
}
TEST_CASE("try catch returns from catch block") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "try_catch_returns_from_catch"), Value::Type::t_object, BLOOPTEXT("catch"));
}
TEST_CASE("try catch returns the exception from catch block") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "try_catch_returns_exception_from_catch"), Value::Type::t_object, BLOOPTEXT("catch"));
}
TEST_CASE("function throws and gets caught in main") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "throw_exception_from_function"), Value::Type::t_object, BLOOPTEXT("catch"));
}
TEST_CASE("function throws and catches it") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "function_catches_exception"), Value::Type::t_object, BLOOPTEXT("caught"));
}
TEST_CASE("assigning an exception") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "assign_an_exception"), Value::Type::t_object, BLOOPTEXT("exception!"));
}
TEST_CASE("stress test") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "stress_test"), Value::Type::t_object, BLOOPTEXT("f1:error|modified|inner:outer|first:second|wrapped:wrapped:wrapped:deep"));
}