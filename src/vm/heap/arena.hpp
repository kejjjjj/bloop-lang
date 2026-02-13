#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{

	class Arena {

	public:
		Arena(bloop::BloopUInt size) {
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
				return nullptr;

			m_pPtr = next;
			return reinterpret_cast<T*>(aligned);
		}

		template<typename T>
		inline T* Alloc() {
			return Alloc(sizeof(T), alignof(T));
		}

		inline void Reset() { m_pPtr = m_pBegin; }

	private:
		bloop::BloopByte* m_pBegin{};
		bloop::BloopByte* m_pEnd{};
		bloop::BloopByte* m_pPtr{};
	};

}