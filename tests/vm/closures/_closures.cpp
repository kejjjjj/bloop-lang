#include "tests/vm/defs.hpp"

#define PREFIX "closures"

TEST_CASE("local capture with a mutation") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "closure_capture_mutation"), Value::Type::t_int, std::array{2,3,3});
}
TEST_CASE("closure capture by reference") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "closure_capture_by_reference"), Value::Type::t_object, BLOOPTEXT("B"));
}
TEST_CASE("returned closure keeps state alive") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "closure_returned"), Value::Type::t_int, std::array{1,2});
}
TEST_CASE("closure instance has isolated state") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "closure_multiple_instances"), Value::Type::t_object, std::array{BLOOPTEXT("A"), BLOOPTEXT("B")});
}
TEST_CASE("nested closures capture outer + inner scopes") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "closure_nested"), Value::Type::t_object, BLOOPTEXT("XYZ"));
}
TEST_CASE("captured variables outlive creator frame") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "closure_lifetime"), Value::Type::t_object, BLOOPTEXT("alive"));
}
TEST_CASE("mutation through multiple closure layers") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "closure_mutation_chain"), Value::Type::t_int, 2);
}
TEST_CASE("recursive closure capturing outer variable") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "recursive_closure_basic"), Value::Type::t_int, 3);
}
TEST_CASE("returned closure used recursively") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "recursive_closure_returned"), Value::Type::t_int, 4);
}
TEST_CASE("recursive closure has isolated state") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "recursive_closure_multiple_instances"), Value::Type::t_int, std::array{2,3});
}
TEST_CASE("recursive call inside nested closure") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "recursive_nested_closure"), Value::Type::t_int, 5);
}
TEST_CASE("mutual recursion via closures") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "recursive_mutual_closure"), Value::Type::t_int, 4);
}
TEST_CASE("closure escapes during recursion") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "recursive_closure_escape"), Value::Type::t_int, 3);
}