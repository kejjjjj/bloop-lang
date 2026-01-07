#include "vm/heap/heap.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/vm.hpp"
#include "utils/hash.hpp"

#include <cassert>
#include <ranges>

using namespace bloop::vm;


Object* Heap::Allocate(Object* newObj) {

	if (ShouldCollect())
		m_pVM->m_oGC.Collect(m_pVM);

	newObj->next = m_pObjects;
	m_pObjects = newObj;
	m_uBytesAllocated += newObj->GetSize();
	return newObj;
}
Object* Heap::AllocString(bloop::BloopUInt len) {
	auto newBuf = new bloop::BloopChar[len];
	return Allocate(new Object(newBuf, len));
}
Object* Heap::AllocString(bloop::BloopChar* data, bloop::BloopUInt len) {
	auto newBuf = new bloop::BloopChar[len];
	std::memcpy(newBuf, data, len);
	auto ptr = Allocate(new Object(newBuf, len));
	ptr->string.hash = bloop::hash::FNV1a(newBuf, len);
	return ptr;
}
Object* Heap::AllocCallable(Function* callable) {
	return Allocate(new Object(callable));
}
Object* Heap::AllocArray(bloop::BloopIndex numValues) {

	auto vals = new Value[numValues];
	auto arr = Allocate(new Object(vals, numValues));
	return arr;
}
Object* Heap::AllocObject(bloop::BloopIndex numValues) {

	auto capacity = numValues <= 1 ? 4 : numValues * numValues;
	auto entries = new ObjectEntry[capacity];

	auto obj = Allocate(new Object(entries, 0, capacity));
	return obj;
}
Object* Heap::AllocClosure(Function* function, bloop::BloopIndex numVals) {
	auto vals = new UpValue*[numVals];
	return Allocate(new Object(function, vals, numVals));
}
Object* Heap::AllocUpValue(Value* slot, UpValue* location) {
	auto up = new UpValue{ nullptr, slot, {}, location };
	auto r = Allocate(new Object(up));
	up->owner = r;
	return r;
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
void Heap::FreeObject(Object* obj)
{
	assert(m_uBytesAllocated >= obj->GetSize());
	m_uBytesAllocated -= obj->GetSize();
	obj->Free();
	delete obj;
}