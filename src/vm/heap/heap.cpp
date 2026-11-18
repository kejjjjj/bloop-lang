#include "vm/heap/heap.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/gc/handles.hpp"
#include "vm/vm.hpp"
#include "std/native.hpp"
#include "utils/hash.hpp"

#include <cassert>
#include <ranges>
#include <cstring>
#include <new>

using namespace bloop::vm;

Object* Heap::AllocString(bloop::BloopUInt len) {
	auto obj = AllocateRaw<Object>(sizeof(Object) + len);

	obj->type = Object::Type::ot_string;
	obj->string.len = len;
	obj->string.data = reinterpret_cast<bloop::BloopChar*>(obj + 1);
	obj->string.hash = {};

	return obj;
}
Object* Heap::AllocString(const bloop::BloopChar* data, bloop::BloopUInt len) {
	auto obj = AllocateRaw<Object>(sizeof(Object) + len);

	obj->type = Object::Type::ot_string;
	obj->string.len = len;
	obj->string.data = reinterpret_cast<bloop::BloopChar*>(obj + 1);
	obj->string.hash = bloop::hash::FNV1a(data, len);

	std::memcpy(obj->string.data, data, len);
	return obj;
}
Object* Heap::AllocCallable(Function* callable) {
	return Allocate<Object>(callable);
}
Object* Heap::AllocArray(bloop::BloopIndex numValues) {
	auto* obj = AllocateRaw<Object>(sizeof(Object) + sizeof(Value) * numValues);

	obj->type = Object::Type::ot_array;
	obj->array.count = numValues;
	obj->array.values = reinterpret_cast<Value*>(obj + 1);

	// the GC may scan the array before the caller fills it
	for (const auto i : std::views::iota(0, static_cast<bloop::BloopInt>(numValues)))
		new (&obj->array.values[i]) Value();

	return obj;
}
Object* Heap::AllocObject(bloop::BloopIndex numValues) {

	// keep the table at most 75% full, so that lookups always find an empty slot
	bloop::BloopInt capacity = 4;
	while (static_cast<bloop::BloopInt>(numValues) * 4 > capacity * 3) capacity <<= 1;

	auto* obj = AllocateRaw<Object>(sizeof(Object) + sizeof(ObjectEntry) * capacity);

	obj->type = Object::Type::ot_object;
	obj->object.capacity = capacity;
	obj->object.count = 0;
	obj->object.entries = reinterpret_cast<ObjectEntry*>(obj + 1);

	for (const auto i : std::views::iota(0, capacity)) {
		obj->object.entries[i] = ObjectEntry{};
	}

	return obj;
}
Object* Heap::AllocClosure(Function* function, bloop::BloopIndex numVals) {

	auto* obj = AllocateRaw<Object>(sizeof(Object) + sizeof(UpValue*) * numVals);

	obj->type = Object::Type::ot_closure;
	obj->closure.function = function;
	obj->closure.numValues = numVals;
	obj->closure.upvalues = reinterpret_cast<UpValue**>(obj + 1);

	// the GC may scan the closure before all of the upvalues have been captured
	for (const auto i : std::views::iota(0u, static_cast<bloop::BloopUInt>(numVals)))
		obj->closure.upvalues[i] = nullptr;

	return obj;
}
Object* Heap::AllocNativeFunction(const bloop::standard::NativeFunction* function) {
	return Allocate<Object>(function);
}
Object* Heap::StringConcat(Object* a, Object* b)
{
	const auto len = a->string.len + b->string.len;

	// the allocation may move a and b
	RootScope scope(*m_pGC);
	const auto ra = scope.Keep(a);
	const auto rb = scope.Keep(b);
	auto r = AllocString(len);
	a = ra.Obj();
	b = rb.Obj();

	memcpy(r->string.data, a->string.data, a->string.len);
	memcpy(r->string.data + a->string.len, b->string.data, b->string.len);
	r->string.hash = bloop::hash::FNV1a(r->string.data, r->string.len);
	return r;
}
void Heap::ObjectSet(Object* obj, Value key, Value value) {

	assert(obj->IsObject() && key.IsString());

	if (obj->ObjectFind(key) == nullptr && obj->ObjectNeedsToGrow()) {

		// the allocation can move obj, key and value
		RootScope scope(*m_pGC);
		const auto robj = scope.Keep(obj);
		const auto rkey = scope.Keep(key);
		const auto rvalue = scope.Keep(value);

		const auto newCapacity = obj->object.capacity * 2;
		auto* entries = m_pGC->AllocateBuffer<ObjectEntry>(static_cast<bloop::BloopUInt>(newCapacity));

		obj = robj.Obj();
		key = rkey.Get();
		value = rvalue.Get();

		for (const auto i : std::views::iota(0, newCapacity))
			new (&entries[i]) ObjectEntry{};

		// rehash into the new table (no allocations from here on)
		const auto* oldEntries = obj->object.entries;
		const auto oldCapacity = obj->object.capacity;

		obj->object.entries = entries;
		obj->object.capacity = newCapacity;
		obj->object.count = 0;

		for (const auto i : std::views::iota(0, oldCapacity)) {
			if (oldEntries[i].key.type != Value::Type::t_undefined)
				obj->ObjectSet(oldEntries[i].key, oldEntries[i].value);
		}

		// an old object now owns a (possibly young) buffer full of values
		m_pGC->WriteBarrierAll(obj);
	}

	obj->ObjectSet(key, value);
	m_pGC->WriteBarrier(obj, key);
	m_pGC->WriteBarrier(obj, value);
}
