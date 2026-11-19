#pragma once

#include "utils/defs.hpp"

#include <vector>

namespace bloop::vm
{
	struct GCHeader;

	// The old generation: a non-moving free-list heap made of chunks.
	// Every block in a chunk (live or free) starts with a GCHeader, so a chunk can be
	// walked linearly. This lets the sweep coalesce neighbouring dead/free blocks and
	// give completely empty chunks back to the OS.
	struct MajorSpace {
		BLOOP_NONCOPYABLE(MajorSpace);

		explicit MajorSpace(bloop::BloopSize initialChunkSize);
		~MajorSpace();

		// never triggers a collection: if no free block fits, the heap grows
		[[nodiscard]] GCHeader* Alloc(bloop::BloopSize payloadSize);

		// frees every unmarked block, clears the marks of the survivors and rebuilds the free list
		void Sweep();

		[[nodiscard]] bool Contains(const void* p) const noexcept;

		// bytes held by live (or not yet swept) blocks, headers included
		[[nodiscard]] inline bloop::BloopSize AllocatedBytes() const noexcept { return m_uAllocatedBytes; }
		[[nodiscard]] inline bloop::BloopSize HeapBytes() const noexcept { return m_uHeapBytes; }
		[[nodiscard]] inline bloop::BloopSize ChunkCount() const noexcept { return m_oChunks.size(); }

		// walks every chunk and checks the block layout invariants (debugging / tests)
		[[nodiscard]] bool Verify() const;

	private:
		struct Chunk {
			bloop::BloopByte* begin{};
			bloop::BloopSize size{};
		};

		[[nodiscard]] GCHeader* TakeFromFreeList(bloop::BloopSize payloadSize);
		void Grow(bloop::BloopSize minBlockSize);
		void PushFree(GCHeader* block, bloop::BloopSize totalSize);

		std::vector<Chunk> m_oChunks;
		GCHeader* m_pFreeList{};

		bloop::BloopSize m_uNextChunkSize{};
		bloop::BloopSize m_uHeapBytes{};
		bloop::BloopSize m_uAllocatedBytes{};
	};

}
