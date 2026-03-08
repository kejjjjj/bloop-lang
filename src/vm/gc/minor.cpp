#include "vm/gc/minor.hpp"
#include "vm/gc/gc.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/value.hpp"

#include <iostream>

using namespace bloop::vm;

void* MinorSpace::AllocSpace(bloop::BloopUInt size, bloop::BloopUInt align)
{
	bloop::BloopUInt ptr = reinterpret_cast<bloop::BloopUInt>(m_pAllocSpace->m_pPtr);
	const auto aligned = (ptr + (align - 1) & ~(align - 1));
	const auto next = reinterpret_cast<bloop::BloopByte*>(aligned + size);

	if (next > m_pAllocSpace->m_pEnd)
		m_oGC.MinorGC();

	m_pAllocSpace->m_pPtr = next;
	return reinterpret_cast<void*>(aligned);
}
void MinorSpace::WriteBarrier(const Object* owner, Object** slot, const Object* value) {
	if (owner->IsOld() && value->IsYoung())
		m_oRememberedSet.push_back(slot);

}
Object* MinorSpace::Copy(GCHeader* old) {

	assert(old);

	if (old->forwarding)
		return old->forwarding->GetValue<Object>();

	if (old->age >= PROMOTION_THRESHOLD)
		return Promote(old);

	const auto totalSize = sizeof(GCHeader) + old->size;
	auto newHeader = reinterpret_cast<GCHeader*>(AllocSpace(totalSize));

	memcpy(newHeader, old, totalSize);
	assert(!newHeader->forwarding);

	old->forwarding = newHeader;
	newHeader->age++;

	return newHeader->GetValue<Object>();
}
Object* MinorSpace::Promote(GCHeader* h) {

	assert(h);

	std::cout << "promotion!\n";

	auto* newObj = m_oGC.m_oMajorSpace.Alloc(h->size);

	// copy object body
	memcpy(newObj + 1, h + 1, h->size);
	newObj->age = h->age;
	newObj->marked = false;
	newObj->forwarding = nullptr;

	h->forwarding = newObj;

	return newObj->GetValue<Object>();
}