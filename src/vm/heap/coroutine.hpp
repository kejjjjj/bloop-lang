#pragma once

#include "vm/value.hpp"
#include "vm/frame.hpp"

#include <vector>

namespace bloop::vm
{
	struct Object;
	struct UpValue;

	// the state of a coroutine (std.coroutine), owned by the GC (GC::m_oCoroutines) and referenced by one Object.
	//
	// A running coroutine lives on the VM's own stacks, right above the native call that resumed it.
	// When it yields, its part of the stack, its frames and the open upvalues that point into it get moved
	// in here, and resuming moves them back on top of the stacks (wherever they are by then).
	//
	// Coroutine objects are always allocated in the old generation, so they never move: open upvalues can
	// point into m_oStack while it's suspended, and keep the coroutine alive through UpValue::owner
	struct CoroutineState {
		enum class Status : bloop::BloopByte {
			suspended, // created, or paused in a yield
			running,   // the one that's running right now
			normal,    // it resumed another coroutine, and is waiting for it to yield or finish
			dead       // it returned, or an exception or an error ended it
		};

		Object* m_pOwner{};
		Value m_oCallee;   // the function it runs (undefined once it has started)
		Status m_eStatus{ Status::suspended };
		bloop::BloopBool m_bStarted{};

		// while suspended. Stack indices (frame and handler bases) are relative to the coroutine's first slot
		std::vector<Value> m_oStack;
		std::vector<CallFrame> m_oFrames;
		UpValue* m_pOpenUpValues{}; // sorted like GC::m_pOpenUpValues, they point into m_oStack

		// while running (or normal)
		Object* m_pResumer{};              // the coroutine that resumed this one (null: the main script)
		bloop::BloopSize m_uCallNesting{}; // VM::m_uCallNesting inside this coroutine: yield only works right there

		[[nodiscard]] static const char* StatusName(Status status) noexcept {
			switch (status) {
			case Status::suspended: return "suspended";
			case Status::running: return "running";
			case Status::normal: return "normal";
			case Status::dead: return "dead";
			}
			return "dead";
		}
	};
}
