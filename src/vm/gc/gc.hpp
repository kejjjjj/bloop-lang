#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{
	struct Object;
	class Heap;
	class VM;

	class GC {
		friend class VM;
	public:

		GC() = delete;
		GC(Heap* heap) : m_pHeap(heap){}

		void Collect(VM* vm);

	private:
		void MarkRoots(VM* vm);
		void Mark(Object* obj);
		void Sweep();
		void Trace(Object* obj);

		Heap* m_pHeap{};
	};
}