#pragma once

#include "utils/defs.hpp"

#include "vm/gc/gc.hpp"

namespace bloop::vm
{

	class Arena {

	public:
		Arena(GC& gc, bloop::BloopUInt size) : m_oGC(gc) {
			m_pBegin = reinterpret_cast<bloop::BloopByte*>(::operator new(size));
			m_pEnd = m_pBegin + size;
			m_pPtr = m_pBegin;
		}
		~Arena() { ::operator delete(m_pBegin); }

		template<typename T>
		inline T* Alloc(bloop::BloopUInt size, bloop::BloopUInt align = alignof(std::max_align_t)) {

			bloop::BloopUInt ptr = reinterpret_cast<bloop::BloopUInt>(m_pPtr);
			const auto aligned = (ptr + (align - 1) & ~(align - 1));
			const auto next = reinterpret_cast<bloop::BloopByte*>(aligned + size);

			if (next > m_pEnd)
				m_oGC.Collect();

			m_pPtr = next;
			return reinterpret_cast<T*>(aligned);
		}

		template<typename T>
		inline T* Alloc() {
			return Alloc<T*>(static_cast<bloop::BloopUInt>(sizeof(T)));
		}

		inline void Reset() { m_pPtr = m_pBegin; }

	private:
		bloop::BloopByte* m_pBegin{};
		bloop::BloopByte* m_pEnd{};
		bloop::BloopByte* m_pPtr{};
		GC& m_oGC;
	};

}