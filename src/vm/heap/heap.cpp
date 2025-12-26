#include "vm/heap/heap.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/vm.hpp"

#include <cassert>

using namespace bloop::vm;


Object* Heap::Allocate(Object* newObj) {

	if (ShouldCollect())
		m_pVM->m_oGC.Collect(m_pVM);

	newObj->next = m_pObjects;
	m_pObjects = newObj;
	m_uBytesAllocated += newObj->GetSize();
	return newObj;
}
Object* Heap::AllocString(std::size_t len) {
	auto newBuf = new char[len];
	return Allocate(new Object(newBuf, len));
}
Object* Heap::AllocString(char* data, std::size_t len) {
	auto newBuf = new char[len];
	std::memcpy(newBuf, data, len);
	return Allocate(new Object(newBuf, len));
}
Object* Heap::AllocCallable(Function* callable) {
	return Allocate(new Object(callable));
}
Object* Heap::StringConcat(Object* a, Object* b)
{
	const auto len = a->string.len + b->string.len;
	auto r = AllocString(len);
	memcpy(r->string.data, a->string.data, a->string.len);
	memcpy(r->string.data + a->string.len, b->string.data, b->string.len);
	return r;
}
void Heap::FreeObject(Object* obj)
{
	assert(m_uBytesAllocated >= obj->GetSize());
	m_uBytesAllocated -= obj->GetSize();
	obj->Free();
	delete obj;
}