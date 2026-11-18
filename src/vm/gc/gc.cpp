#include "vm/gc/gc.hpp"
#include "vm/gc/trace.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_set>

using namespace bloop::vm;

GC::GC(VM* vm) : m_pVM(vm), m_oMinorSpace(NURSERY_SIZE), m_oMajorSpace(MAJOR_CHUNK_SIZE) {
	m_oTempRoots.reserve(BLOOP_MAX_FRAMES);
}
GC::~GC() = default;

GCHeader* GC::AllocHeader(bloop::BloopUInt payloadSize, GCKind kind) {

	payloadSize = bloop::Align(payloadSize);

	if (m_uPretenure || payloadSize > LARGE_OBJECT_SIZE)
		return AllocOld(payloadSize, kind);

#ifdef BLOOP_GC_STRESS
	// collect on every allocation so that missing roots / barriers show up immediately
	if (CanCollect()) {
		if (++m_uStressCounter % BLOOP_GC_STRESS_MAJOR_INTERVAL == 0u)
			MajorGC();
		else
			MinorGC();
	}
#endif

	const auto total = sizeof(GCHeader) + payloadSize;
	void* mem = m_oMinorSpace.TryAlloc(total);

	if (!mem && CanCollect()) {
		MinorGC();
		MaybeMajorGC();
		mem = m_oMinorSpace.TryAlloc(total);
	}

	// the nursery is still full of survivors, put the object straight into the old generation
	if (!mem)
		return AllocOld(payloadSize, kind);

	auto* h = static_cast<GCHeader*>(mem);
	h->size = payloadSize;
	h->forwarding = nullptr;
	h->nextFree = nullptr;
	h->age = 0u;
	h->kind = kind;
	h->marked = false;
	h->remembered = false;
	h->free = false;
	return h;
}
GCHeader* GC::AllocOld(bloop::BloopUInt payloadSize, GCKind kind) {

	// collect *before* allocating, the new object isn't initialized yet
	if (CanCollect())
		MaybeMajorGC();

	auto* h = m_oMajorSpace.Alloc(payloadSize);
	h->kind = kind;
	m_oStats.bytesPretenured += h->TotalSize();

	// the object will be initialized with values that may well be young
	if (kind == GCKind::object)
		Remember(h);

	return h;
}
void GC::MaybeMajorGC() {
	if (m_oMajorSpace.AllocatedBytes() >= m_uNextMajorAt)
		MajorGC();
}
void GC::Remember(GCHeader* h) {
	assert(h->IsOld() && !h->remembered);
	h->remembered = true;
	m_oRememberedSet.push_back(h);
}
void GC::PushTempRoot(Object* obj) {
	m_oTempRoots.push_back(obj);
}
void GC::PopTempRoot(bloop::BloopUInt count) {
	assert(count <= m_oTempRoots.size());
	while (count-- && !m_oTempRoots.empty())
		m_oTempRoots.pop_back();
}
Object* GC::TempRoot(bloop::BloopUInt fromTop) const {
	assert(fromTop < m_oTempRoots.size());
	return m_oTempRoots[m_oTempRoots.size() - 1u - fromTop];
}
void GC::WriteUpValue(UpValue* uv, const Value& value) {
	*uv->location = value;
	if (uv->IsClosed())
		WriteBarrier(uv, value);
}

/***********************************************************************
 *                              DEBUGGING
 ***********************************************************************/

#ifndef NDEBUG
bool GC::Verify() {

	if (!m_oMajorSpace.Verify())
		return false;

	bool ok = true;

	// every reachable young object must be in the active semispace, and every
	// old object pointing to one must be in the remembered set
	const auto checkSlot = [&](GCHeader* owner, Object* child) {
		const auto* ch = GCHeader::GetHeader(child);
		if (ch->IsYoung()) {
			if (!m_oMinorSpace.InToSpace(ch))
				ok = false;
			if (owner && owner->IsOld() && !owner->remembered)
				ok = false;
		} else if (!m_oMajorSpace.Contains(ch) || ch->free) {
			ok = false;
		}
	};

	std::vector<GCHeader*> stack;
	std::unordered_set<GCHeader*> seen;

	const auto push = [&](GCHeader* h) {
		if (seen.insert(h).second)
			stack.push_back(h);
	};

	ForEachRoot([&](Object*& slot) { checkSlot(nullptr, slot); push(GCHeader::GetHeader(slot)); });
	for (auto* uv = m_pOpenUpValues; uv; uv = uv->next)
		push(GCHeader::GetHeader(uv));

	while (!stack.empty()) {
		auto* h = stack.back();
		stack.pop_back();
		detail::VisitChildren(h,
			[&](Object*& slot) { checkSlot(h, slot); push(GCHeader::GetHeader(slot)); },
			[&](UpValue* uv) {
				const auto* uh = GCHeader::GetHeader(uv);
				if (!uh->IsOld() || uh->kind != GCKind::upvalue)
					ok = false;
				push(GCHeader::GetHeader(uv));
			},
			[&](auto*& buffer) {
				auto* bh = GCHeader::GetHeader(buffer);
				if (bh->kind != GCKind::buffer)
					ok = false;
				else if (bh->IsYoung() ? !m_oMinorSpace.InToSpace(bh) || (h->IsOld() && !h->remembered) : !m_oMajorSpace.Contains(bh) || bh->free)
					ok = false;
			});
	}

	return ok;
}
#endif
