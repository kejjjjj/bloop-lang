#include "tests/vm/defs.hpp"

#define PREFIX "algorithms"

TEST_CASE("fibonacci computed recursively for n=10 is 55") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "fibonacci_recursive"), Value::Type::t_int, 55);
}
TEST_CASE("fibonacci computed iteratively for n=10 is 55") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "fibonacci_iterative"), Value::Type::t_int, 55);
}
TEST_CASE("factorial computed recursively for n=10 is 3628800") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "factorial_recursive"), Value::Type::t_int, 3628800);
}
TEST_CASE("gcd of 48 and 18 is 6") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "gcd"), Value::Type::t_int, 6);
}
TEST_CASE("2 to the power of 10 is 1024") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "power"), Value::Type::t_int, 1024);
}
TEST_CASE("bubble sort of [5,3,1,4,2] produces [1,2,3,4,5]") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "bubble_sort"), Value::Type::t_int, std::array{1, 2, 3, 4, 5});
}
TEST_CASE("binary search finds 7 at index 3 in [1,3,5,7,9,11]") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "binary_search"), Value::Type::t_int, 3);
}
TEST_CASE("17 is prime") {
    bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "is_prime"), Value::Type::t_int, 1);
}
