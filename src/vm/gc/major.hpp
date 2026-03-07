#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{
	struct GCHeader;
	class GC;

	struct FreeBlock {
		bloop::BloopUInt size;
		FreeBlock* next;
	};

	//long lived objects
	struct MajorSpace {

		MajorSpace(GC& gc, bloop::BloopUInt size = MB) : m_oGC(gc){
			Grow(size);
		}

		GCHeader* objects;
		FreeBlock* free_list;

		[[nodiscard]] GCHeader* Alloc(bloop::BloopUInt payloadSize);
		[[nodiscard]] FreeBlock* FindFree(bloop::BloopUInt totalSize);

	private:
		void Grow(bloop::BloopUInt minSize);
		[[nodiscard]] GCHeader* UseBlock(FreeBlock* block, bloop::BloopUInt needed);

		GC& m_oGC;
		bloop::BloopUInt m_uHeapSize{};
	};

}