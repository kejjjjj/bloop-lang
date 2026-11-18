#include "tests/vm/defs.hpp"
#include "std/bind.hpp"
#include "vm/gc/handles.hpp"

#include <iostream>
#include <sstream>
#include <string>

using namespace bloop::standard;

// natives used only by these tests, registered as the global "testlib"
namespace {
	bloop::BloopInt Add(bloop::BloopInt a, bloop::BloopInt b) { return a + b; }
	bloop::BloopDouble Half(bloop::BloopDouble x) { return x / 2.0; }
	bloop::BloopBool Not(bloop::BloopBool b) { return !b; }
	bloop::BloopUInt Twice(bloop::BloopUInt u) { return u * 2u; }

	// expected failures are return values, not errors
	std::optional<bloop::BloopInt> ParseDigit(StringArg s) {
		if (s.Length() != 1 || s.View()[0] < '0' || s.View()[0] > '9')
			return std::nullopt;
		return s.View()[0] - '0';
	}

	bloop::BloopString Kind(std::variant<StringArg, ArrayArg, bloop::BloopInt> v) {
		switch (v.index()) {
		case 0: return "string";
		case 1: return "array";
		default: return "int";
		}
	}

	bloop::BloopInt Sum(NativeArgs args) {
		bloop::BloopInt total{};
		for (const auto& v : args) {
			if (v.type != Value::Type::t_int)
				throw bloop::exception::VMError("sum only adds ints");
			total += v.i;
		}
		return total;
	}

	bloop::BloopString Join(StringArg separator, NativeArgs rest) {
		bloop::BloopString result;
		for (std::size_t i = 0; i < rest.size(); i++) {
			if (i)
				result += separator.View();
			result += rest[i].ValueToString();
		}
		return result;
	}

	// allocates a lot (and forces collections) while holding on to its arguments
	Value RepeatWithGC(bloop::vm::VM& vm, StringArg text, ArrayArg numbers, bloop::BloopInt times) {
		auto& heap = vm.GetHeap();
		bloop::vm::RootScope scope(vm.GetGC());
		auto result = scope.Keep(heap.AllocString("", 0u));

		for (bloop::BloopInt i = 0; i < times; i++) {
			// the views are re-read after every allocation, so moving objects is fine
			const auto piece = text.Str() + std::to_string(numbers[i % numbers.Count()].i);
			auto* pieceObj = heap.AllocString(piece.data(), piece.size());
			result.Set(Value{ heap.StringConcat(result.Obj(), pieceObj) });

			if (i % 3 == 0)
				vm.MinorGC();
			if (i % 10 == 0)
				vm.MajorGC();
		}
		return result.Get();
	}

	bloop::BloopUInt Collections(bloop::vm::VM& vm) { return vm.GetGC().Stats().minorCollections; }

	void RegisterTestNatives() {
		static const bool registered = [] {
			g_Natives.push_back(NativeDef(BLOOPTEXT("testlib"), NativeObject{
				.m_oFields = {
					NativeField(BLOOPTEXT("add"), Bind<&Add>()),
					NativeField(BLOOPTEXT("half"), Bind<&Half>()),
					NativeField(BLOOPTEXT("not"), Bind<&Not>()),
					NativeField(BLOOPTEXT("twice"), Bind<&Twice>()),
					NativeField(BLOOPTEXT("parse_digit"), Bind<&ParseDigit>()),
					NativeField(BLOOPTEXT("kind"), Bind<&Kind>()),
					NativeField(BLOOPTEXT("sum"), Bind<&Sum>()),
					NativeField(BLOOPTEXT("join"), Bind<&Join>()),
					NativeField(BLOOPTEXT("repeat_with_gc"), Bind<&RepeatWithGC>()),
					NativeField(BLOOPTEXT("collections"), Bind<&Collections>()),
				}
			}));
			return true;
		}();
		(void)registered;
	}

	// runs a script, returns its value and what it printed (runtime errors are printed)
	std::pair<Value, std::string> Run(const std::string& src) {
		RegisterTestNatives();

		std::ostringstream captured;
		auto* old = std::cout.rdbuf(captured.rdbuf());
		auto ret = bloop::test::TEST_ExecuteBuffer(src);
		std::cout.rdbuf(old);

		REQUIRE(ret.has_value());
		return { *ret, captured.str() };
	}
	void RequireError(const std::string& src, const std::string& message) {
		const auto [ret, output] = Run(src);
		INFO(output);
		REQUIRE(ret.type == Value::Type::t_undefined); // runtime errors terminate the script
		REQUIRE(output.find(message) != std::string::npos);
	}
	void RequireString(const Value& v, const std::string& expected) {
		REQUIRE(v.IsString());
		REQUIRE(std::string(v.obj->string.data, v.obj->string.len) == expected);
	}
}

TEST_CASE("bindings | numbers are converted") {
	auto [ret, out] = Run("return [testlib.add(2, 3), testlib.add(2u, 3), testlib.half(3), testlib.twice(4), testlib.not(false)];");
	REQUIRE(ret.IsArray());
	const auto* v = ret.obj->array.values;
	REQUIRE(v[0].type == Value::Type::t_int);
	REQUIRE(v[0].i == 5);
	REQUIRE(v[1].i == 5);
	REQUIRE(v[2].type == Value::Type::t_double);
	REQUIRE(v[2].d == 1.5);
	REQUIRE(v[3].type == Value::Type::t_uint);
	REQUIRE(v[3].u == 8u);
	REQUIRE(v[4].type == Value::Type::t_bool);
	REQUIRE(v[4].b);
}
TEST_CASE("bindings | wrong argument types terminate the script") {
	RequireError("return testlib.add(1, \"2\");", "argument 2 must be \"int\", but got \"string\"");
	RequireError("return testlib.add(\"1\", \"2\");", "argument 1 must be \"int\"");
	RequireError("return testlib.half([]);", "argument 1 must be \"number\", but got \"array\"");
	RequireError("return testlib.not(1);", "argument 1 must be \"boolean\", but got \"int\"");
	RequireError("return testlib.twice(-1);", "argument 1 must be \"uint\", but got \"int\"");
}
TEST_CASE("bindings | wrong argument counts terminate the script") {
	RequireError("return testlib.add(1);", "passed 1 arguments, but expected 2");
	RequireError("return testlib.join();", "passed 0 arguments, but expected at least 1");
}
TEST_CASE("bindings | std::optional returns undefined for expected failures") {
	auto [ret, out] = Run("return [testlib.parse_digit(\"7\"), testlib.parse_digit(\"x\"), testlib.parse_digit(\"12\")];");
	const auto* v = ret.obj->array.values;
	REQUIRE(v[0].type == Value::Type::t_int);
	REQUIRE(v[0].i == 7);
	REQUIRE(v[1].type == Value::Type::t_undefined);
	REQUIRE(v[2].type == Value::Type::t_undefined);
}
TEST_CASE("bindings | variants accept any of their alternatives") {
	auto [ret, out] = Run("return [testlib.kind(\"a\"), testlib.kind([1]), testlib.kind(5)];");
	RequireString(ret.obj->array.values[0], "string");
	RequireString(ret.obj->array.values[1], "array");
	RequireString(ret.obj->array.values[2], "int");

	RequireError("return testlib.kind(1.5);", "argument 1 must be \"string\" or \"array\" or \"int\", but got \"double\"");
}
TEST_CASE("bindings | variadic natives") {
	auto [ret, out] = Run("return [testlib.sum(), testlib.sum(1, 2, 3), testlib.join(\"-\", \"a\", 1, \"b\"), testlib.join(\",\")];");
	const auto* v = ret.obj->array.values;
	REQUIRE(v[0].i == 0);
	REQUIRE(v[1].i == 6);
	RequireString(v[2], "a-1-b");
	RequireString(v[3], "");
}
TEST_CASE("bindings | std.length uses the bindings") {
	RequireError("return std.length(5);", "argument 1 must be \"string\" or \"array\", but got \"int\"");
}
TEST_CASE("bindings | argument views survive allocations and collections") {
	auto [ret, out] = Run(
		"let text = \"ab\" + \"c\";\n"         // young, will move
		"let numbers = [1, 2, 3];\n"
		"let before = testlib.collections();\n"
		"let result = testlib.repeat_with_gc(text, numbers, 30);\n"
		"return [result, testlib.collections() > before];");

	std::string expected;
	for (int i = 0; i < 30; i++)
		expected += "abc" + std::to_string(i % 3 + 1);

	RequireString(ret.obj->array.values[0], expected);
	REQUIRE(ret.obj->array.values[1].b);
}
