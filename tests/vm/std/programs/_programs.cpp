#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("std", "programs")

TEST_CASE("std | word count program") {
    // the: 3, cat: 2, and: 2 (cat was seen first), then dog, bird, saw, a
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "word_count"), Value::Type::t_object, BLOOPTEXT("the,cat,and|7"));
}
