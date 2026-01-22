#pragma once

#include "utils/defs.hpp"

#include <vector>

namespace bloop::vm
{
	struct Object;
	class Heap;
	class VM;

	class GC {
		BLOOP_NONCOPYABLE(GC);
		friend class VM;
	public:

		GC() = delete;
		GC(Heap* heap);

		void Collect(VM* vm);

		void Pause() { m_bIsPaused = true; }
		void Continue() { m_bIsPaused = false; }

		void PushTempRoot(Object* obj);
		void PopTempRoot(bloop::BloopUInt count=1u);

	private:
		void MarkRoots(VM* vm);
		void Mark(Object* obj);
		void Sweep();
		void Trace(Object* obj);

		Heap* m_pHeap{};
		bool m_bIsPaused{};
		std::vector<Object*> m_oTempRoots;
	};
}