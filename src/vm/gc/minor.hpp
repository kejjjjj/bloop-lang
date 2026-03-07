#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{
	struct Object;
	struct GCHeader;
	class GC;
	

	struct Space {

		Space(bloop::BloopUInt size) {
			m_pBegin = reinterpret_cast<bloop::BloopByte*>(::operator new(size));
			m_pEnd = m_pBegin + size;
			m_pPtr = m_pBegin;
		}
		~Space() { ::operator delete(m_pBegin); }

		inline void Reset() { m_pPtr = m_pBegin; }

		bloop::BloopByte* m_pBegin{};
		bloop::BloopByte* m_pEnd{};
		bloop::BloopByte* m_pPtr{};
	};

	struct MinorSpace {
		friend class GC;

		MinorSpace(GC& gc, bloop::BloopUInt size=MB) : m_oFromSpace(size), m_oToSpace(size * 2), m_oGC(gc) {}

		[[nodiscard]] void* AllocFromSpace(bloop::BloopUInt size, bloop::BloopUInt align = alignof(std::max_align_t));
		[[nodiscard]] void* AllocToSpace(bloop::BloopUInt size, bloop::BloopUInt align = alignof(std::max_align_t));

		inline void SwapSpaces() {
			std::swap(m_oFromSpace, m_oToSpace);
			m_oToSpace.Reset();
		}

	private:
		[[maybe_unused]] Object* Copy(GCHeader* obj);
		[[nodiscard]] Object* Promote(GCHeader* h);

		Space m_oFromSpace;
		Space m_oToSpace;
		GC& m_oGC;
	};

}
