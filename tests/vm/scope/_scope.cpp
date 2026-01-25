#include "tests/vm/defs.hpp"

#define PREFIX "scope"

TEST_CASE("unnamed scope") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "unnamed_scope"), Value::Type::t_object, BLOOPTEXT("scope!"));
}
TEST_CASE("double unnamed scope") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "double_unnamed_scope"), Value::Type::t_object, BLOOPTEXT("scope!2"));
}
TEST_CASE("unnamed scope assignment") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "unnamed_scope_assignment"), Value::Type::t_object, BLOOPTEXT("scope!3"));
}
TEST_CASE("closure references 'deleted' variables") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "closure"), Value::Type::t_int, std::array{1, 1});
}
