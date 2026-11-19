#include "tests/vm/defs.hpp"
#include "vm/heap/dvalue.hpp"

#define PREFIX "gc"

namespace {
	const GCStats& LastStats() {
		auto* vm = bloop::test::TEST_GetLastVM();
		REQUIRE(vm != nullptr);
		return vm->GetGC().Stats();
	}
	const Value& Element(const Value& arr, bloop::BloopInt idx) {
		REQUIRE(arr.IsArray());
		REQUIRE(idx < arr.obj->array.count);
		return arr.obj->array.values[idx];
	}
	void RequireString(const Value& v, const bloop::BloopString& expected) {
		REQUIRE(v.IsString());
		REQUIRE(bloop::BloopString(v.obj->string.data, static_cast<std::size_t>(v.obj->string.len)) == expected);
	}
	Value Run(const bloop::BloopString& name) {
		auto ret = bloop::test::TEST_ExecuteFile(bloop::BloopString(BLOOPTEXT(PREFIX)) + BLOOP_DIRECTORY_SEPARATOR + name + BLOOPTEXT(".bloop"));
		REQUIRE(ret.has_value());
		return *ret;
	}
}

TEST_CASE("gc | short lived garbage is collected by minor collections") {
	bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "many_temporaries"), Value::Type::t_int, 49950000);
	REQUIRE(LastStats().minorCollections > 0u);
}
TEST_CASE("gc | young values stored into an old array survive") {
	const auto ret = Run("old_to_young_array");
	REQUIRE(Element(ret, 0).type == Value::Type::t_int);
	REQUIRE(Element(ret, 0).i == 3);
	RequireString(Element(ret, 1), "young string");
	RequireString(Element(ret, 2), "nested!");
}
TEST_CASE("gc | young values stored into an old object survive") {
	const auto ret = Run("old_to_young_object");
	REQUIRE(Element(ret, 0).type == Value::Type::t_int);
	REQUIRE(Element(ret, 0).i == 5);
	RequireString(Element(ret, 1), "b2");
}
TEST_CASE("gc | cyclic structures") {
	bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "cycles"), Value::Type::t_bool, std::array{ true, true, true });
}
TEST_CASE("gc | old closed upvalues holding young values") {
	const auto ret = Run("closure_upvalue");
	REQUIRE(Element(ret, 0).type == Value::Type::t_int);
	REQUIRE(Element(ret, 0).i == 3);
	RequireString(Element(ret, 1), "cccc");
}
TEST_CASE("gc | the closure being called is kept alive by its frame") {
	bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "callee_survives"), Value::Type::t_object, BLOOPTEXT("hi!?"));
}
TEST_CASE("gc | closures created in a loop") {
	bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "closures_in_loop"), Value::Type::t_int, std::array{ 101, 20001, 30000 });
}
TEST_CASE("gc | large strings bypass the nursery") {
	const auto ret = Run("large_string");
	REQUIRE(Element(ret, 0).type == Value::Type::t_int);
	REQUIRE(Element(ret, 0).i == 262144);
	REQUIRE(Element(ret, 1).type == Value::Type::t_bool);
	REQUIRE(Element(ret, 1).b);
	REQUIRE(Element(ret, 2).b);
	REQUIRE(LastStats().bytesPretenured >= 262144u);
}
TEST_CASE("gc | large arrays with young elements") {
	// (array literals that big would overflow the parser, so drive the heap directly)
	REQUIRE(bloop::test::TEST_ExecuteBuffer(BLOOPTEXT("return 0;")).has_value());
	auto* vm = bloop::test::TEST_GetLastVM();
	auto& gc = vm->GetGC();
	auto& heap = vm->GetHeap();

	// big enough to be allocated straight into the old generation
	constexpr bloop::BloopIndex count = 12000; // (Value is only 12 bytes on some 32-bit ABIs)
	static_assert(sizeof(Object) + sizeof(Value) * count > LARGE_OBJECT_SIZE);

	gc.PushTempRoot(heap.AllocArray(count));
	REQUIRE(gc.TempRoot()->IsOld());

	for (bloop::BloopIndex i = 0; i < count; i++) {
		const auto text = std::to_string(i);
		auto* str = heap.AllocString(text.data(), text.size()); // young, may move the array
		gc.TempRoot()->array.values[i] = Value{ str };
		gc.WriteBarrier(gc.TempRoot(), str);

		if (i % 1000 == 0)
			gc.MinorGC();
	}

	gc.MinorGC();
	gc.MinorGC();
	gc.MajorGC();
	gc.MinorGC();

	const Value arr{ gc.TempRoot() };
	gc.PopTempRoot();

	for (bloop::BloopIndex i = 0; i < count; i += 999)
		RequireString(Element(arr, i), std::to_string(i));
	RequireString(Element(arr, count - 1), std::to_string(count - 1));
#ifndef NDEBUG
	REQUIRE(gc.Verify());
#endif
}
TEST_CASE("gc | long lived data gets promoted and survives major collections") {
	bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "heap_growth"), Value::Type::t_int, std::array{ 150000, 74925000 });

	const auto& stats = LastStats();
	REQUIRE(stats.bytesPromoted > 0u);
	REQUIRE(stats.majorCollections > 0u);
}
TEST_CASE("gc | the old generation gets reused after major collections") {
	bloop::test::CheckConstant(MAKE_RELATIVE_PATH(PREFIX, "heap_shrinks"), Value::Type::t_int, 59999);

	auto* vm = bloop::test::TEST_GetLastVM();
	const auto& old = vm->GetGC().OldSpace();
	INFO("old generation: " << old.HeapBytes() << " bytes in " << old.ChunkCount() << " chunks, " << old.AllocatedBytes() << " bytes live");
	INFO("promoted: " << vm->GetGC().Stats().bytesPromoted);
	// the program promotes way more than the heap can ever hold at once
	REQUIRE(vm->GetGC().Stats().bytesPromoted > old.HeapBytes());
}
TEST_CASE("gc | old objects that grow a young entry buffer") {
	const auto ret = Run("object_growth");
	RequireString(Element(ret, 0), "b1");
	RequireString(Element(ret, 1), "c");
	RequireString(Element(ret, 2), "e!");
	RequireString(Element(ret, 3), "g?");
	RequireString(Element(ret, 4), "h#");
}
