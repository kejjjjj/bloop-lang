#include "vm/gc/minor.hpp"
#include "vm/gc/gc.hpp"

using namespace bloop::vm;

void* MinorSpace::AllocFromSpace(bloop::BloopUInt size, bloop::BloopUInt align)
{
	bloop::BloopUInt ptr = reinterpret_cast<bloop::BloopUInt>(m_oFromSpace.m_pPtr);
	const auto aligned = (ptr + (align - 1) & ~(align - 1));
	const auto next = reinterpret_cast<bloop::BloopByte*>(aligned + size);

	if (next > m_oFromSpace.m_pEnd)
		m_oGC.MinorGC();

	m_oFromSpace.m_pPtr = next;
	return reinterpret_cast<void*>(aligned);
}
void* MinorSpace::AllocToSpace(bloop::BloopUInt size, bloop::BloopUInt align)
{
	bloop::BloopUInt ptr = reinterpret_cast<bloop::BloopUInt>(m_oFromSpace.m_pPtr);
	const auto aligned = (ptr + (align - 1) & ~(align - 1));
	const auto next = reinterpret_cast<bloop::BloopByte*>(aligned + size);

	if (next > m_oFromSpace.m_pEnd) {
		assert(false);
		return nullptr;
	}

	m_oFromSpace.m_pPtr = next;
	return reinterpret_cast<void*>(aligned);
}
Object* MinorSpace::Copy(GCHeader* old) {

	assert(old);

	if (old->forwarding)
		return old->forwarding->GetValue<Object>();

	if (old->age >= PROMOTION_THRESHOLD)
		return Promote(old);

	const auto totalSize = sizeof(GCHeader) + old->size;
	auto newHeader = reinterpret_cast<GCHeader*>(AllocToSpace(totalSize));

	memcpy(newHeader, old, totalSize);
	assert(!newHeader->forwarding);

	old->forwarding = newHeader;
	newHeader->age++;

	return newHeader->GetValue<Object>();
}
Object* MinorSpace::Promote(GCHeader* h) {

	assert(h);

	auto* newObj = m_oGC.m_oMajorSpace.Alloc(h->size);
	memcpy(newObj, h, sizeof(GCHeader) + h->size);
	h->forwarding = newObj;
	return newObj->GetValue<Object>();
}