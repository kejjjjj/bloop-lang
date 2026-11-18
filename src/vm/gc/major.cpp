#include "vm/gc/major.hpp"
#include "vm/gc/gc.hpp"
#include "vm/gc/trace.hpp"

#include <algorithm>
#include <cstring>
#include <new>

using namespace bloop::vm;

namespace {
	constexpr auto HEADER = sizeof(GCHeader);
	// don't leave behind free fragments that can't hold anything useful
	constexpr auto MIN_SPLIT = HEADER + alignof(std::max_align_t);
	constexpr bloop::BloopUInt MAX_CHUNK_SIZE = 64u * bloop::MB;

	inline void InitFreeBlock(GCHeader* h, bloop::BloopUInt totalSize) noexcept {
		h->size = totalSize - HEADER;
		h->forwarding = nullptr;
		h->nextFree = nullptr;
		h->age = OLD_AGE;
		h->kind = GCKind::object;
		h->marked = false;
		h->remembered = false;
		h->free = true;
	}
}

MajorSpace::MajorSpace(bloop::BloopUInt initialChunkSize) : m_uNextChunkSize(bloop::Align(initialChunkSize)) {
	Grow(m_uNextChunkSize);
}
MajorSpace::~MajorSpace() {
	for (auto& chunk : m_oChunks)
		::operator delete(chunk.begin);
}
GCHeader* MajorSpace::Alloc(bloop::BloopUInt payloadSize) {

	payloadSize = bloop::Align(payloadSize);

	if (auto* h = TakeFromFreeList(payloadSize))
		return h;

	Grow(HEADER + payloadSize);

	auto* h = TakeFromFreeList(payloadSize);
	assert(h && "a freshly grown chunk must fit the allocation");
	return h;
}
GCHeader* MajorSpace::TakeFromFreeList(bloop::BloopUInt payloadSize) {

	const auto needed = HEADER + payloadSize;

	// first fit
	for (auto** link = &m_pFreeList; *link; link = &(*link)->nextFree) {
		auto* block = *link;
		assert(block->free && "non-free block in the free list");

		const auto total = block->TotalSize();
		if (total < needed)
			continue;

		*link = block->nextFree;

		// split it if the remainder is still usable, otherwise hand out the whole block
		if (total - needed >= MIN_SPLIT) {
			auto* rest = reinterpret_cast<GCHeader*>(reinterpret_cast<bloop::BloopByte*>(block) + needed);
			PushFree(rest, total - needed);
			block->size = payloadSize;
		}

		block->free = false;
		block->marked = false;
		block->remembered = false;
		block->forwarding = nullptr;
		block->nextFree = nullptr;
		block->age = OLD_AGE;
		block->kind = GCKind::object;

		m_uAllocatedBytes += block->TotalSize();
		return block;
	}

	return nullptr;
}
void MajorSpace::Grow(bloop::BloopUInt minBlockSize)
{
	const auto size = bloop::Align(std::max(minBlockSize, m_uNextChunkSize));
	m_uNextChunkSize = std::min(m_uNextChunkSize * 2u, MAX_CHUNK_SIZE);

	// operator new returns memory aligned to at least __STDCPP_DEFAULT_NEW_ALIGNMENT__ (== alignof(max_align_t))
	auto* mem = static_cast<bloop::BloopByte*>(::operator new(size));
	m_oChunks.push_back({ mem, size });
	m_uHeapBytes += size;

	PushFree(reinterpret_cast<GCHeader*>(mem), size);
}
void MajorSpace::PushFree(GCHeader* block, bloop::BloopUInt totalSize) {
	assert(totalSize >= HEADER);
	InitFreeBlock(block, totalSize);
	block->nextFree = m_pFreeList;
	m_pFreeList = block;
}
void MajorSpace::Sweep() {

	m_pFreeList = nullptr;
	m_uAllocatedBytes = 0u;

	std::vector<Chunk> keep;
	keep.reserve(m_oChunks.size());
	auto remaining = m_oChunks.size();

	for (auto& chunk : m_oChunks) {

		auto* p = chunk.begin;
		auto* const end = chunk.begin + chunk.size;

		GCHeader* run = nullptr;   // start of the current run of dead/free blocks
		bloop::BloopUInt runSize{};

		auto flush = [&]() {
			if (!run)
				return;
			PushFree(run, runSize);
			run = nullptr;
			runSize = 0u;
		};

		while (p < end) {
			auto* h = reinterpret_cast<GCHeader*>(p);
			const auto total = h->TotalSize();
			assert(total >= HEADER && p + total <= end && "corrupted old generation block");

			if (h->free || !h->marked) {
#ifndef NDEBUG
				if (!h->free) // poison dead objects so that dangling pointers fail loudly
					std::memset(h->GetValue<void>(), 0xDD, h->size);
#endif
				if (!run)
					run = h;
				runSize += total;
			} else {
				flush();
				assert(!h->remembered && "the remembered set must be empty during a major GC");
				h->marked = false;
				m_uAllocatedBytes += total;
			}

			p += total;
		}

		// the whole chunk is garbage, give it back (but always keep at least one chunk around)
		if (run == reinterpret_cast<GCHeader*>(chunk.begin) && runSize == chunk.size && remaining > 1u) {
			::operator delete(chunk.begin);
			m_uHeapBytes -= chunk.size;
			remaining--;
			continue;
		}

		flush();
		keep.push_back(chunk);
	}

	m_oChunks = std::move(keep);
}
bool MajorSpace::Contains(const void* p) const noexcept {
	const auto* b = static_cast<const bloop::BloopByte*>(p);
	for (const auto& chunk : m_oChunks) {
		if (b >= chunk.begin && b < chunk.begin + chunk.size)
			return true;
	}
	return false;
}
bool MajorSpace::Verify() const {

	bloop::BloopUInt allocated{};
	bloop::BloopUInt heap{};

	for (const auto& chunk : m_oChunks) {
		const auto* p = chunk.begin;
		const auto* end = chunk.begin + chunk.size;
		heap += chunk.size;

		while (p < end) {
			const auto* h = reinterpret_cast<const GCHeader*>(p);
			if (h->size % alignof(std::max_align_t))
				return false;
			if (h->TotalSize() > static_cast<bloop::BloopUInt>(end - p))
				return false;
			if (h->age != OLD_AGE || h->forwarding)
				return false;
			if (!h->free)
				allocated += h->TotalSize();
			p += h->TotalSize();
		}
		if (p != end)
			return false;
	}

	for (const auto* f = m_pFreeList; f; f = f->nextFree) {
		if (!f->free || !Contains(f))
			return false;
	}

	return allocated == m_uAllocatedBytes && heap == m_uHeapBytes;
}

/***********************************************************************
 *                              MAJOR GC
 ***********************************************************************/

void GC::MajorGC() {

	// empty the nursery first, so that the mark & sweep only has to care about the old generation
	MinorCollect(true);

	assert(m_oMinorSpace.Used() == 0u && "tenuring everything should empty the nursery");
	assert(m_oRememberedSet.empty() && "no young objects -> no old to young pointers");

	m_bCollecting = true;
	m_oStats.majorCollections++;

	MarkRoots();
	DrainMarkStack();

	m_oMajorSpace.Sweep();

	// next major collection when the old generation has doubled
	m_uNextMajorAt = std::max(MAJOR_MIN_THRESHOLD, m_oMajorSpace.AllocatedBytes() * 2u);

	m_bCollecting = false;

#if defined(BLOOP_GC_VERIFY) && !defined(NDEBUG)
	assert(Verify() && "heap verification failed after a major GC");
#endif
}
void GC::MarkRoots() {
	ForEachRoot([this](Object*& slot) { MarkHeader(GCHeader::GetHeader(slot)); });

	// open upvalues are reachable through the open upvalue list
	for (auto* uv = m_pOpenUpValues; uv; uv = uv->next)
		MarkHeader(GCHeader::GetHeader(uv));
}
void GC::MarkHeader(GCHeader* h) {
	assert(h->IsOld() && !h->free && "the major GC only sees old, allocated blocks");
	if (h->marked)
		return;

	h->marked = true;
	m_oWorkList.push_back(h);
}
void GC::DrainMarkStack() {
	while (!m_oWorkList.empty()) {
		auto* h = m_oWorkList.back();
		m_oWorkList.pop_back();

		detail::VisitChildren(h,
			[this](Object*& slot) { MarkHeader(GCHeader::GetHeader(slot)); },
			[this](UpValue* uv) { MarkHeader(GCHeader::GetHeader(uv)); },
			[this](auto*& buffer) { MarkHeader(GCHeader::GetHeader(buffer)); });
	}
}
