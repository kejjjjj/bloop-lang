#include "tests/vm/defs.hpp"

#include <string>

// the parser used to recurse once per comma, so long lists overflowed the C++ stack

TEST_CASE("long array literal (20000 elements)") {
    constexpr auto count = 20000;

    bloop::BloopString src = BLOOPTEXT("return [");
    for (auto i = 0; i < count; i++) {
        if (i)
            src += BLOOPTEXT(", ");
        src += std::to_string(i);
    }
    src += BLOOPTEXT("];");

    const auto ret = bloop::test::TEST_ExecuteBuffer(src);
    REQUIRE(ret.has_value());
    REQUIRE(ret->IsArray());
    REQUIRE(ret->obj->array.count == count);

    for (auto i = 0; i < count; i += 997) {
        REQUIRE(ret->obj->array.values[i].type == Value::Type::t_int);
        REQUIRE(ret->obj->array.values[i].i == i);
    }
    REQUIRE(ret->obj->array.values[count - 1].i == count - 1);
}
TEST_CASE("long array literal of expressions and nested arrays") {
    constexpr auto count = 2000; // (a chunk can hold at most 65535 bytes of bytecode)

    bloop::BloopString src = BLOOPTEXT("let x = 2;\nreturn [");
    for (auto i = 0; i < count; i++)
        src += BLOOPTEXT("[x * ") + std::to_string(i) + BLOOPTEXT(", \"s\" + \"t\"], ");
    src.resize(src.size() - 2); // no trailing comma
    src += BLOOPTEXT("];");

    const auto ret = bloop::test::TEST_ExecuteBuffer(src);
    REQUIRE(ret.has_value());
    REQUIRE(ret->IsArray());
    REQUIRE(ret->obj->array.count == count);

    const auto& last = ret->obj->array.values[count - 1];
    REQUIRE(last.IsArray());
    REQUIRE(last.obj->array.values[0].i == 2 * (count - 1));
    REQUIRE(last.obj->array.values[1].IsString());
}
TEST_CASE("long sequence expression in a for loop header still works") {
    const auto ret = bloop::test::TEST_ExecuteBuffer(BLOOPTEXT("let a = 0; let b = 0; let c = 0;\na = 1, b = 2, c = 3;\nreturn [a, b, c];"));
    REQUIRE(ret.has_value());
    REQUIRE(ret->IsArray());
    REQUIRE(ret->obj->array.values[0].i == 1);
    REQUIRE(ret->obj->array.values[1].i == 2);
    REQUIRE(ret->obj->array.values[2].i == 3);
}
