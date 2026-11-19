#include "vm/gc/gc.hpp"
#include "vm/gc/trace.hpp"

#include <cstring>

using namespace bloop::vm;

/***********************************************************************
 *                              MINOR GC
 ***********************************************************************/

void GC::MinorGC() {
	MinorCollect(false);
}
void GC::MinorCollect(bool tenureAll) {

	assert(!m_bCollecting && "recursive garbage collection");
	m_bCollecting = true;
	m_bTenureAll = tenureAll;
	m_oStats.minorCollections++;

	m_oMinorSpace.Flip();
	assert(m_oWorkList.empty());

	// 1. roots
	ForEachRoot([this](Object*& slot) { EvacuateObject(slot); });

	// 2. old objects that (may) point into the nursery.
	//    Scanning them can remember them again (for the next cycle) if they still point to young objects
	auto remembered = std::move(m_oRememberedSet);
	m_oRememberedSet.clear();

	for (auto* h : remembered) {
		h->remembered = false;
		ScanRemembered(h);
	}

	// 3. everything reachable from the evacuated objects (Cheney style, but with a work list
	//    because promoted objects don't live in to-space)
	while (!m_oWorkList.empty()) {
		auto* h = m_oWorkList.back();
		m_oWorkList.pop_back();

		if (h->IsOld())
			ScanRemembered(h);
		else
			ScanEvacuated(h);
	}

#ifndef NDEBUG
	// nothing may point into from-space anymore
	auto& from = m_oMinorSpace.Reserve();
	std::memset(from.Begin(), 0xCD, from.Used());
#endif

	m_bTenureAll = false;
	m_bCollecting = false;

#if defined(BLOOP_GC_VERIFY) && !defined(NDEBUG)
	assert(Verify() && "heap verification failed after a minor GC");
#endif
}
GCHeader* GC::Evacuate(GCHeader* h) {

	if (!m_oMinorSpace.InFromSpace(h))
		return h; // old object (or already in to-space)

	if (h->forwarding)
		return h->forwarding;

	assert(h->kind != GCKind::upvalue && "upvalues never live in the nursery");

	const auto total = h->TotalSize();
	const auto age = static_cast<bloop::BloopUInt8>(h->age + 1u);

	GCHeader* n = nullptr;

	if (!m_bTenureAll && age < PROMOTION_THRESHOLD)
		n = static_cast<GCHeader*>(m_oMinorSpace.TryAlloc(total));

	if (n) {
		std::memcpy(n, h, total);
		n->age = age;
		m_oStats.bytesSurvived += total;
	} else {
		// promote (or the to-space overflowed, which can only happen with heavy fragmentation)
		n = m_oMajorSpace.Alloc(h->size);
		assert(n->size >= h->size && "the old generation may hand out a slightly bigger block");
		std::memcpy(n->GetValue<void>(), h->GetValue<void>(), h->size);
		n->kind = h->kind;
		n->age = OLD_AGE;
		m_oStats.bytesPromoted += total;
	}

	n->forwarding = nullptr;
	n->nextFree = nullptr;
	n->marked = false;
	n->remembered = false;
	n->free = false;

	detail::RelocateInternalPointers(h, n);

	h->forwarding = n;
	m_oWorkList.push_back(n);
	return n;
}
void GC::EvacuateObject(Object*& obj) {
	assert(obj);
	obj = Evacuate(GCHeader::GetHeader(obj))->GetValue<Object>();
}
void GC::ScanEvacuated(GCHeader* h) {
	// closures only reference upvalues, which are always old
	detail::VisitChildren(h,
		[this](Object*& slot) { EvacuateObject(slot); },
		[](UpValue*) {},
		[this](auto*& buffer) { EvacuateBuffer(buffer); });
}
void GC::ScanRemembered(GCHeader* h) {

	bool pointsToYoung = false;

	detail::VisitChildren(h,
		[&](Object*& slot) {
			EvacuateObject(slot);
			pointsToYoung |= GCHeader::GetHeader(slot)->IsYoung();
		},
		[](UpValue*) {},
		[&](auto*& buffer) {
			EvacuateBuffer(buffer);
			pointsToYoung |= GCHeader::GetHeader(buffer)->IsYoung();
		});

	if (pointsToYoung && !h->remembered)
		Remember(h);
}
