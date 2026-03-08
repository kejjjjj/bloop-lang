#pragma once

#include "utils/defs.hpp"

#include <vector>

namespace bloop::vm
{
	struct Object;
	struct GCHeader;
	class GC;
	

	struct Space {
		BLOOP_NONCOPYABLE(Space);

		Space(bloop::BloopUInt size) {
			m_pBegin = reinterpret_cast<bloop::BloopByte*>(::operator new(size));
			m_pEnd = m_pBegin + size;
			m_pPtr = m_pBegin;
		}
		~Space() { ::operator delete(m_pBegin); }

		inline void Reset() { m_pPtr = m_pBegin; }

		Space(Space&& other) noexcept {
			*this = std::move(other);
		}

		Space& operator=(Space&& other) noexcept {
			if (this != &other) {
				::operator delete(m_pBegin);

				m_pBegin = other.m_pBegin;
				m_pEnd = other.m_pEnd;
				m_pPtr = other.m_pPtr;

				other.m_pBegin = nullptr;
				other.m_pEnd = nullptr;
				other.m_pPtr = nullptr;
			}
			return *this;
		}

		bloop::BloopByte* m_pBegin{};
		bloop::BloopByte* m_pEnd{};
		bloop::BloopByte* m_pPtr{};
	};

	struct MinorSpace {
		friend class GC;

		MinorSpace(GC& gc, bloop::BloopUInt size=MB) : m_spaceA(size), m_spaceB(size), m_oGC(gc) {
			m_pAllocSpace = &m_spaceA;
			m_pScanSpace = &m_spaceB;
		}

		[[nodiscard]] void* AllocSpace(bloop::BloopUInt size, bloop::BloopUInt align = alignof(std::max_align_t));

		inline void SwapSpaces() {
			std::swap(m_pAllocSpace, m_pScanSpace);
			m_pAllocSpace->Reset();
		}

		void WriteBarrier(const Object* owner, Object** slot, const Object* value);

	private:
		[[maybe_unused]] Object* Copy(GCHeader* obj);
		[[nodiscard]] Object* Promote(GCHeader* h);

		Space m_spaceA;
		Space m_spaceB;

		Space* m_pAllocSpace{};
		Space* m_pScanSpace{};

		std::vector<Object**> m_oRememberedSet;

		GC& m_oGC;
	};

}
