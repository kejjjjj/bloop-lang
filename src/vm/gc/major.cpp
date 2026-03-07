#include "vm/gc/major.hpp"
#include "vm/gc/gc.hpp"

using namespace bloop::vm;

GCHeader* MajorSpace::Alloc(bloop::BloopUInt payloadSize) {

	const auto needed = sizeof(GCHeader) + payloadSize;

	// do we have a free block somewhere
	if (const auto block = FindFree(needed))
		return UseBlock(block, needed);

	// call the GC to see if we can free up some space
	m_oGC.MajorGC();

	if (const auto block = FindFree(needed))
		return UseBlock(block, needed);

	// if not, grow the heap
	Grow(needed);

	//find free space from the new heap space
	if (const auto block = FindFree(needed))
		return UseBlock(block, needed);

	assert(false);
	return nullptr;
}
FreeBlock* MajorSpace::FindFree(bloop::BloopUInt needed) {

	auto** block = &free_list;

	while (*block) {

		if ((*block)->size >= needed) {
			auto* result = *block;
			*block = result->next;
			return result; // reuse memory
		}

		block = &(*block)->next;
	}

	return nullptr;

}
void MajorSpace::Grow(bloop::BloopUInt minSize)
{
	const auto growSize = std::max(minSize, m_uHeapSize * 2);

	auto mem = new char[growSize];
	assert(mem);

	auto* block = reinterpret_cast<FreeBlock*>(mem);
	block->size = growSize;
	block->next = free_list;

	m_uHeapSize += growSize;
}
GCHeader* MajorSpace::UseBlock(FreeBlock* block, bloop::BloopUInt needed)
{
	const auto remaining = block->size - needed;

	// split it if possible
	// + 16 to give the whole block rather than give an unusable free fragment
	if (remaining > sizeof(FreeBlock) + 16) {
		auto* newFree = reinterpret_cast<FreeBlock*>(reinterpret_cast<bloop::BloopUInt8*>(block) + needed);
		newFree->size = remaining;
		newFree->next = free_list;
		free_list = newFree;
		block->size = needed;
	}

	auto* header = reinterpret_cast<GCHeader*>(block);

	header->size = needed - sizeof(GCHeader);
	header->marked = false;
	header->age = PROMOTION_THRESHOLD; // it's old gen now
	header->forwarding = nullptr;

	header->next = objects;
	objects = header;

	return header;
}