#include "tests/vm/std/helpers.hpp"

#include <string>

using namespace bloop::test::std_lib;

namespace {
	// `count` statements (about 14 bytes of bytecode each)
	std::string Repeat(const std::string& statement, int count) {
		std::string result;
		for (auto i = 0; i < count; i++)
			result += statement;
		return result;
	}
}

// jump targets used to be 16 bits, so a chunk (all of the top level code of a program, or one function)
// couldn't be larger than 64KB. Every construct below jumps over or back across about 70KB
TEST_CASE("large chunks | jumps over more than 64KB of bytecode") {
	const auto body = Repeat("x = x + 1;\n", 5000);

	Expect("let x = 0;\nlet i = 0;\nwhile (i < 3) { i++;\n" + body + "}\nreturn x;", "15000");
	Expect("let x = 0;\nfor (let i = 0; i < 2; i++) {\n" + body + "}\nreturn x;", "10000");
	Expect("let x = 0;\nfor (const k : [1, 2]) {\n" + body + "}\nreturn x;", "10000");
	Expect("let x = 0;\nif (x === 0) {\n" + body + "} else { x = -1; }\nreturn x;", "5000");
	Expect("let x = 0;\nif (x === 1) {\n" + body + "} else { x = -1; }\nreturn x;", "-1");
	Expect("let x = 0;\ntry {\n" + body + "throw 1;\n} catch (e) { x += e; }\nreturn x;", "5001");
	Expect("const g = () => { let y = 0;\n" + Repeat("y = y + 1;\n", 5000) + "return y; };\nreturn g();", "5000"); // a lambda
	Expect("fn f() { let x = 0;\nfor (let i = 0; i < 2; i++) {\n" + body + "}\nreturn x; }\nreturn f();", "10000");
	Expect("let x = 0;\nlet i = 0;\nwhile (true) { i++;\n" + body + "if (i === 2) { break; } }\nreturn x;", "10000");
}
