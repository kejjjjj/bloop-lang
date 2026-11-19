#include "tests/vm/defs.hpp"
#include "vm/gc/handles.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"

#include <string>

namespace {
	bloop::vm::VM& FreshVM() {
		REQUIRE(bloop::test::TEST_ExecuteBuffer(BLOOPTEXT("return 0;")).has_value());
		auto* vm = bloop::test::TEST_GetLastVM();
		REQUIRE(vm);
		return *vm;
	}
	std::string Text(const Value& v) {
		REQUIRE(v.IsString());
		return std::string(v.obj->string.data, static_cast<std::size_t>(v.obj->string.len));
	}
}

TEST_CASE("handles | locals follow their objects through collections") {
	auto& vm = FreshVM();
	auto& gc = vm.GetGC();
	auto& heap = vm.GetHeap();

	RootScope scope(gc);
	std::vector<Local> strings;
	for (int i = 0; i < 100; i++) {
		const auto text = "string " + std::to_string(i);
		strings.push_back(scope.Keep(heap.AllocString(text.data(), text.size())));
	}

	const auto* before = strings[0].Obj();
	const auto wasYoung = before->IsYoung(); // (GC stress builds have promoted it already)
	gc.MinorGC();
	gc.MinorGC();
	gc.MajorGC();

	if (wasYoung)
		REQUIRE(strings[0].Obj() != before); // it moved (nursery -> nursery -> old generation)
	REQUIRE(strings[0].Obj()->IsOld());
	for (int i = 0; i < 100; i++)
		REQUIRE(Text(strings[i].Get()) == "string " + std::to_string(i));
#ifndef NDEBUG
	REQUIRE(gc.Verify());
#endif
}
TEST_CASE("handles | primitives and Set") {
	auto& vm = FreshVM();
	auto& gc = vm.GetGC();

	RootScope scope(gc);
	auto local = scope.Keep(Value{ bloop::BloopInt{ 42 } });
	REQUIRE(!local.IsObject());
	REQUIRE(local.Get().i == 42);

	local.Set(Value{ vm.GetHeap().AllocString("x", 1u) });
	REQUIRE(local.IsObject());
	gc.MinorGC();
	REQUIRE(Text(local.Get()) == "x");

	local.Set(Value{ true });
	REQUIRE(!local.IsObject());
	gc.MinorGC(); // the slot is empty now
	REQUIRE(local.Get().b);
}
TEST_CASE("handles | scopes release their roots, also when an exception unwinds") {
	auto& vm = FreshVM();
	auto& gc = vm.GetGC();
	const auto base = gc.TempRootCount();

	{
		RootScope outer(gc);
		[[maybe_unused]] auto a = outer.Keep(vm.GetHeap().AllocString("a", 1u));
		{
			RootScope inner(gc);
			[[maybe_unused]] auto b = inner.Keep(vm.GetHeap().AllocString("b", 1u));
			[[maybe_unused]] auto c = inner.Keep(vm.GetHeap().AllocString("c", 1u));
			REQUIRE(gc.TempRootCount() == base + 3u);
		}
		REQUIRE(gc.TempRootCount() == base + 1u);
	}
	REQUIRE(gc.TempRootCount() == base);

	REQUIRE_THROWS_AS([&] {
		RootScope scope(gc);
		[[maybe_unused]] auto a = scope.Keep(vm.GetHeap().AllocString("a", 1u));
		throw bloop::exception::VMError("boom");
	}(), bloop::exception::VMError);
	REQUIRE(gc.TempRootCount() == base);
}
