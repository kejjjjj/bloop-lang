#pragma once

#include "utils/defs.hpp"
#include "vm/gc/arena.hpp"

#include <utility>

namespace bloop::vm
{
	// The young generation: two equally sized arenas (semispaces).
	// New objects are bump allocated into the active arena. A minor GC flips the
	// arenas and evacuates the survivors from the reserve arena (from-space) into the
	// fresh active arena (to-space), or promotes them into the old generation.
	struct MinorSpace {
		BLOOP_NONCOPYABLE(MinorSpace);

		explicit MinorSpace(bloop::BloopUInt size) : m_oSpaceA(size), m_oSpaceB(size) {}

		[[nodiscard]] inline void* TryAlloc(bloop::BloopUInt size) noexcept { return m_pActive->TryAlloc(size); }

		// start of a minor GC: the active arena becomes from-space, the reserve becomes the empty to-space
		inline void Flip() noexcept {
			std::swap(m_pActive, m_pReserve);
			m_pActive->Reset();
		}

		[[nodiscard]] inline bool InToSpace(const void* p) const noexcept { return m_pActive->Contains(p); }
		[[nodiscard]] inline bool InFromSpace(const void* p) const noexcept { return m_pReserve->Contains(p); }

		[[nodiscard]] inline bloop::BloopUInt Capacity() const noexcept { return m_pActive->Capacity(); }
		[[nodiscard]] inline bloop::BloopUInt Used() const noexcept { return m_pActive->Used(); }

		[[nodiscard]] inline Arena& Active() noexcept { return *m_pActive; }
		[[nodiscard]] inline Arena& Reserve() noexcept { return *m_pReserve; }

	private:
		Arena m_oSpaceA;
		Arena m_oSpaceB;

		Arena* m_pActive{ &m_oSpaceA };
		Arena* m_pReserve{ &m_oSpaceB };
	};

}
