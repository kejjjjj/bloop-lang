#include "vm/heap/heap.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/vm.hpp"
#include "std/native.hpp"
#include "utils/hash.hpp"

#include <cassert>
#include <ranges>

using namespace bloop::vm;


//Object* Heap::Allocate(Object* newObj) {
//
//	if (ShouldCollect())
//		m_pVM->m_oGC.Collect(m_pVM);
//
//
//	newObj->next = m_pObjects;
//	m_pObjects = newObj;
//	m_uBytesAllocated += newObj->GetSize();
//	return newObj;
//}
Object* Heap::AllocString(bloop::BloopUInt len) {
	auto newBuf = new bloop::BloopChar[len];
	auto str = Allocate<Object>(newBuf, len);
	m_pGC->AddExternalBytes(len);
	return str;
}
Object* Heap::AllocString(const bloop::BloopChar* data, bloop::BloopUInt len) {
	auto newBuf = new bloop::BloopChar[len];
	std::memcpy(newBuf, data, len);
	auto ptr = Allocate<Object>(newBuf, len);
	ptr->string.hash = bloop::hash::FNV1a(newBuf, len);
	m_pGC->AddExternalBytes(len);
	return ptr;
}
Object* Heap::AllocCallable(Function* callable) {
	return Allocate<Object>(callable);
}
Object* Heap::AllocArray(bloop::BloopIndex numValues) {

	auto vals = new Value[numValues];
	auto arr = Allocate<Object>(vals, numValues);
	m_pGC->AddExternalBytes(sizeof(Value) * numValues);
	return arr;
}
Object* Heap::AllocObject(bloop::BloopIndex numValues) {

	auto capacity = 4;
	while (capacity < numValues) capacity <<= 1;
	auto entries = new ObjectEntry[capacity];
	auto obj = Allocate<Object>(entries, 0, capacity);

	m_pGC->AddExternalBytes(sizeof(ObjectEntry) * capacity);
	return obj;
}
Object* Heap::AllocClosure(Function* function, bloop::BloopIndex numVals) {
	auto vals = new UpValue*[numVals] {};
	auto closure = Allocate<Object>(function, vals, numVals);
	m_pGC->AddExternalBytes(sizeof(UpValue*) * numVals);
	return closure;
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
