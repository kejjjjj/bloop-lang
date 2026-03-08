#include "vm/heap/heap.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/vm.hpp"
#include "std/native.hpp"
#include "utils/hash.hpp"

#include <cassert>
#include <ranges>
#include <cstring>

using namespace bloop::vm;

Object* Heap::AllocString(bloop::BloopUInt len) {
	auto obj = AllocateRaw<Object>(sizeof(Object) + len);

	obj->type = Object::Type::ot_string;
	obj->string.len = len;
	obj->string.data = reinterpret_cast<bloop::BloopChar*>(obj + 1);

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

	return obj;
}
Object* Heap::AllocObject(bloop::BloopIndex numValues) {

	auto capacity = 4;
	while (capacity < numValues) capacity <<= 1;

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

	return obj;
}
Object* Heap::AllocNativeFunction(const bloop::standard::NativeFunction* function) {
	return Allocate<Object>(function);
}
Object* Heap::StringConcat(Object* a, Object* b)
{
	const auto len = a->string.len + b->string.len;
	auto r = AllocString(len);
	memcpy(r->string.data, a->string.data, a->string.len);
	memcpy(r->string.data + a->string.len, b->string.data, b->string.len);
	r->string.hash = bloop::hash::FNV1a(r->string.data, r->string.len);
	return r;
}
