#pragma once

#include "utils/defs.hpp"

#include <new>
#include <cassert>

namespace bloop::vm
{
	// A fixed-size bump allocator.
	// It never collects by itself: when it runs out of room TryAlloc returns nullptr
	// and the owner (the GC) decides what to do (collect, tenure, grow...)
	class Arena {
		BLOOP_NONCOPYABLE(Arena);
	public:
		explicit Arena(bloop::BloopSize size) {
			m_pBegin = static_cast<bloop::BloopByte*>(::operator new(size));
			m_pEnd = m_pBegin + size;
			m_pPtr = m_pBegin;
		}
		~Arena() { ::operator delete(m_pBegin); }

		[[nodiscard]] inline void* TryAlloc(bloop::BloopSize size, bloop::BloopSize align = alignof(std::max_align_t)) noexcept {
			assert((align & (align - 1)) == 0 && "alignment must be a power of 2");

			const auto ptr = reinterpret_cast<bloop::BloopSize>(m_pPtr);
			const auto aligned = (ptr + (align - 1)) & ~(align - 1);

			// compare sizes rather than pointers so that we never form an out of bounds pointer
			const auto end = reinterpret_cast<bloop::BloopSize>(m_pEnd);
			if (aligned > end || size > end - aligned)
				return nullptr;

			m_pPtr = reinterpret_cast<bloop::BloopByte*>(aligned + size);
			return reinterpret_cast<void*>(aligned);
		}

		inline void Reset() noexcept { m_pPtr = m_pBegin; }

		[[nodiscard]] inline bool Contains(const void* p) const noexcept {
			const auto b = reinterpret_cast<const bloop::BloopByte*>(p);
			return b >= m_pBegin && b < m_pEnd;
		}

		[[nodiscard]] inline bloop::BloopByte* Begin() const noexcept { return m_pBegin; }
		[[nodiscard]] inline bloop::BloopByte* Top() const noexcept { return m_pPtr; }
		[[nodiscard]] inline bloop::BloopSize Used() const noexcept { return static_cast<bloop::BloopSize>(m_pPtr - m_pBegin); }
		[[nodiscard]] inline bloop::BloopSize Capacity() const noexcept { return static_cast<bloop::BloopSize>(m_pEnd - m_pBegin); }

	private:
		bloop::BloopByte* m_pBegin{};
		bloop::BloopByte* m_pEnd{};
		bloop::BloopByte* m_pPtr{};
	};

}
