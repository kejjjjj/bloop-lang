#include "vm/heap/heap.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/coroutine.hpp"
#include "vm/gc/handles.hpp"
#include "vm/vm.hpp"
#include "std/native.hpp"
#include "utils/hash.hpp"

#include <cassert>
#include <ranges>
#include <cstring>
#include <algorithm>
#include <new>

using namespace bloop::vm;

Object* Heap::AllocString(bloop::BloopSize len) {
	auto obj = AllocateRaw<Object>(sizeof(Object) + len);

	obj->type = Object::Type::ot_string;
	obj->string.len = len;
	obj->string.data = reinterpret_cast<bloop::BloopChar*>(obj + 1);
	obj->string.hash = {};

	return obj;
}
Object* Heap::AllocString(const bloop::BloopChar* data, bloop::BloopSize len) {
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
Object* Heap::AllocArray(bloop::BloopSize numValues) {
	auto* obj = AllocateRaw<Object>(sizeof(Object) + sizeof(Value) * numValues);

	obj->type = Object::Type::ot_array;
	obj->array.count = static_cast<bloop::BloopInt>(numValues);
	obj->array.capacity = static_cast<bloop::BloopInt>(numValues);
	obj->array.values = reinterpret_cast<Value*>(obj + 1);

	// the GC may scan the array before the caller fills it
	for (bloop::BloopSize i = 0; i < numValues; i++)
		new (&obj->array.values[i]) Value();

	return obj;
}
Object* Heap::ArrayReserve(Object* arr, bloop::BloopInt capacity) {
	assert(arr->IsArray());

	if (capacity <= arr->array.capacity)
		return arr;

	auto newCapacity = std::max<bloop::BloopInt>(arr->array.capacity * 2, 4);
	while (newCapacity < capacity)
		newCapacity *= 2;

	RootScope scope(*m_pGC);
	const auto rarr = scope.Keep(arr);
	auto* values = m_pGC->AllocateBuffer<Value>(static_cast<bloop::BloopSize>(newCapacity));
	arr = rarr.Obj();

	for (bloop::BloopInt i = 0; i < newCapacity; i++)
		new (&values[i]) Value(i < arr->array.count ? arr->array.values[i] : Value{});

	arr->array.values = values;
	arr->array.capacity = newCapacity;

	// an old array now owns a (possibly young) buffer full of values
	m_pGC->WriteBarrierAll(arr);
	return arr;
}
Object* Heap::ArrayPush(Object* arr, Value value) {

	if (arr->array.count == arr->array.capacity) {
		RootScope scope(*m_pGC);
		const auto rvalue = scope.Keep(value);
		arr = ArrayReserve(arr, arr->array.count + 1);
		value = rvalue.Get();
	}

	arr->array.values[arr->array.count++] = value;
	m_pGC->WriteBarrier(arr, value);
	return arr;
}
Object* Heap::AllocObject(bloop::BloopIndex numValues) {

	// keep the table at most 75% full, so that lookups always find an empty slot
	bloop::BloopInt capacity = 4;
	while (static_cast<bloop::BloopInt>(numValues) * 4 > capacity * 3) capacity <<= 1;

	auto* obj = AllocateRaw<Object>(sizeof(Object) + sizeof(ObjectEntry) * static_cast<bloop::BloopSize>(capacity));

	obj->type = Object::Type::ot_object;
	obj->object.capacity = capacity;
	obj->object.count = 0;
	obj->object.nextOrder = 0;
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
Object* Heap::AllocCoroutine(const Value& callee) {
	// the object first: the allocation may collect, and a state without an owner must not be in the registry yet
	auto* h = m_pGC->AllocOld(sizeof(Object), GCKind::object); // (remembers it: the callee may be young)
	auto* obj = new (h->GetValue<void>()) Object(static_cast<CoroutineState*>(nullptr));

	auto* state = new CoroutineState();
	state->m_pOwner = obj;
	state->m_oCallee = callee; // read after the allocation, which may have moved it
	obj->coroutine = state;
	m_pGC->m_oCoroutines.push_back(state);
	return obj;
}
Object* Heap::StringConcat(Object* a, Object* b)
{
	const auto len = a->string.len + b->string.len;

	// the allocation may move a and b
	RootScope scope(*m_pGC);
	const auto ra = scope.Keep(a);
	const auto rb = scope.Keep(b);
	auto r = AllocString(static_cast<bloop::BloopSize>(len));
	a = ra.Obj();
	b = rb.Obj();

	memcpy(r->string.data, a->string.data, static_cast<std::size_t>(a->string.len));
	memcpy(r->string.data + a->string.len, b->string.data, static_cast<std::size_t>(b->string.len));
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
		auto* entries = m_pGC->AllocateBuffer<ObjectEntry>(static_cast<bloop::BloopSize>(newCapacity));

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
			if (oldEntries[i].key.type != Value::Type::t_undefined) {
				auto& entry = obj->ObjectInsertNew(oldEntries[i].key);
				entry.value = oldEntries[i].value;
				entry.order = oldEntries[i].order; // keeps the insertion order
			}
		}

		// an old object now owns a (possibly young) buffer full of values
		m_pGC->WriteBarrierAll(obj);
	}

	obj->ObjectSet(key, value);
	m_pGC->WriteBarrier(obj, key);
	m_pGC->WriteBarrier(obj, value);
}
