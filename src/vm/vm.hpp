#pragma once

#include <vector>
#include <unordered_map>
#include <cassert>
#include <span>

#include "vm/vm_defs.hpp"

#include "metadata/metadata.hpp"
#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "std/environment.hpp"
#include "utils/random.hpp"

#include <optional>
#include <exception>
#include <cstdint>
#include <chrono>
#include <memory>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace bloop::bytecode {
	enum class EOpCode : unsigned char;
}

namespace bloop::vm {
	struct CallFrame;
	struct CoroutineState;
	class TaskList;

	// where the C++ stack is right now (the frame, not a local: sanitizers can move locals off the stack)
	[[nodiscard]] inline std::uintptr_t NativeStackAddress() noexcept {
#if defined(_MSC_VER)
		return reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress());
#else
		return reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
#endif
	}

	// limits for scripts that the host doesn't trust (0: no limit)
	struct Limits {
		// instructions that one Run() or CallFromHost() may execute (functions it calls included)
		bloop::BloopUInt m_uInstructions{};
		// bytes that the old generation may hold after a full collection (the nursery, 2 x 1MB, isn't counted)
		bloop::BloopSize m_uHeapBytes{};

		// bytes of the C++ stack that the script may use. Script functions calling each other don't use it (that's
		// BLOOP_MAX_FRAMES), but a native function calling back into the script (std.array.transform, the host...)
		// recurses in C++, and so would a script recursing through such callbacks. That's a runtime error
		// instead of a crash. On by default: a thread usually has 1MB (windows) or 8MB (linux) of stack,
		// and the host needs some of it too. 0: no check
		bloop::BloopSize m_uStackBytes{ 512u * 1024u };
	};

	class VM {
		friend class GC;
		friend class Heap;
		friend struct CallFrame;

	public:
		// the environment decides which parts of the standard library the script can use (see std/environment.hpp)
		VM(bloop::metadata::Metadata& metadata, bloop::standard::Environment environment = {});
		~VM();

		[[maybe_unused]] Value Run();

		// set when the script ended with std.process.exit(code)
		[[nodiscard]] std::optional<bloop::BloopInt> GetExitCode() const noexcept { return m_oExitCode; }
		// whether the script was terminated by a runtime error
		[[nodiscard]] bool Failed() const noexcept { return m_bFailed; }
		[[nodiscard]] const bloop::standard::Environment& GetEnvironment() const noexcept { return m_oEnvironment; }
		// (std.time.now counts from here)
		[[nodiscard]] std::chrono::steady_clock::time_point GetCreationTime() const noexcept { return m_oCreationTime; }
		// std.time.now(): Environment::m_fnClock, or seconds since the VM was created
		[[nodiscard]] double Now() const;
		// std.math.random's generator (seeded with Environment::m_oRandomSeed)
		[[nodiscard]] bloop::Random& GetRandom() noexcept { return m_oRandom; }

		// ends the script from a native function (throws, Run() returns undefined and GetExitCode() has the code)
		void Exit(bloop::BloopInt code);

		// ---- embedding (see bloop.hpp, which wraps all of this) ----

		void SetLimits(const Limits& limits);
		[[nodiscard]] const Limits& GetLimits() const noexcept { return m_oLimits; }

		// runtime errors are printed to std::cout by default. A host can turn that off and read GetError() instead
		void SetPrintErrors(bool print) noexcept { m_bPrintErrors = print; }
		// the message of the last runtime error (with the stack trace)
		[[nodiscard]] const bloop::BloopString& GetError() const noexcept { return m_sError; }
		// the last runtime error without the trace, and the trace: where it happened first, then the calls that led there
		[[nodiscard]] const bloop::BloopString& GetErrorMessage() const noexcept { return m_sErrorMessage; }
		[[nodiscard]] const std::vector<bc::InstrDebugRef>& GetErrorTrace() const noexcept { return m_oErrorTrace; }

		// calls a script function from the host after Run(): runtime errors and std.process.exit end the call
		// (not the host). Returns nothing then, see Failed(), GetError() and GetExitCode()
		[[nodiscard]] std::optional<Value> CallFromHost(const Value& callee, std::span<const Value> args);

		// resumes a suspended coroutine from the host after Run(), like CallFromHost: runtime errors, an exception
		// that the coroutine doesn't catch and std.process.exit end the call (and the coroutine). Returns what it
		// yielded or returned, or nothing then. `args` follow the rules of ResumeCoroutine
		[[nodiscard]] std::optional<Value> ResumeFromHost(Object* coroutine, std::span<const Value> args);

		// std.task's tasks (vm/tasks.hpp)
		[[nodiscard]] TaskList& GetTasks() noexcept { return *m_pTasks; }

		// whatever the host wants its native functions to reach (a game, a mod...)
		void SetUserData(void* data) noexcept { m_pUserData = data; }
		// (bloop.hpp's own data: its event handlers)
		void SetEmbedding(void* data) noexcept { m_pEmbedding = data; }
		[[nodiscard]] void* GetEmbedding() const noexcept { return m_pEmbedding; }
		template<typename T = void>
		[[nodiscard]] T* GetUserData() const noexcept { return static_cast<T*>(m_pUserData); }

		// a global by its slot (Metadata::m_oExports has the slots of the main file's exports)
		[[nodiscard]] const Value& GetGlobal(bloop::BloopIndex slot) const noexcept {
			assert(slot < m_oGlobals.size());
			return m_oGlobals[slot];
		}
		[[nodiscard]] bool IsRunning() const noexcept { return !m_oFrames.empty(); }
		[[nodiscard]] bloop::metadata::Metadata& GetMetadata() noexcept { return m_refMetaData; }

		void MajorGC() { m_oGC.MajorGC(); }
		void MinorGC() { m_oGC.MinorGC(); }

		// Calls a bloop function, closure or native function from C++ (for natives that take callbacks)
		// and returns its result. The argument count must match, like any call.
		// A throw inside the callee can be caught inside the callee; if it isn't, the script terminates:
		// exceptions can't unwind through the native code that made the call
		[[nodiscard]] Value Call(const Value& callee, std::span<const Value> args);

		[[nodiscard]] GC& GetGC() noexcept { return m_oGC; }
		[[nodiscard]] Heap& GetHeap() noexcept { return m_oHeap; }

		// ---- coroutines (vm/coroutine.cpp), for std.coroutine ----

		// runs a suspended coroutine until it yields (returns the yielded value) or returns (returns its value).
		// The first resume passes `args` to its function, the later ones pass at most one value: the result of
		// the yield it's paused in. An exception that the coroutine doesn't catch ends it and gets thrown again
		// where it was resumed. Only for natives (it may RaiseFromNative)
		[[nodiscard]] Value ResumeCoroutine(Object* coroutine, std::span<const Value> args);
		// pauses the running coroutine once the native that calls this returns, and gives `value` to its resumer.
		// A runtime error outside of a coroutine, or inside a callback that a native function is running
		void YieldCoroutine(const Value& value);
		// the running coroutine (null: the main script)
		[[nodiscard]] Object* RunningCoroutine() const noexcept { return m_pCoroutine; }

		// makes the native that calls this throw `value` as a script exception (one that a try can catch)
		// when it returns. The native's return value is ignored then
		void RaiseFromNative(const Value& value);

		// the one string for a handle type's name (HandleData::type points at it, compared by address). It lives as
		// long as the VM
		[[nodiscard]] const bloop::BloopString* GetHandleType(bloop::BloopStringView name);

		// the native function being called (read it first thing: calls the native makes change it)
		[[nodiscard]] const bloop::standard::NativeFunction* GetCalledNative() const noexcept { return m_pCalledNative; }

	private:
		enum class ExecutionReturnCode : bloop::BloopByte {
			rc_continue,
			rc_return,
			rc_return_value,
			rc_yield // the running coroutine yielded
		};

		void RunGlobal();
		// runs the frames until the frame count drops back to stopDepth, which leaves the return value of
		// the last one on the stack. Script functions calling each other stay in this loop (CALL pushes a frame,
		// RETURN pops it), so they don't use the C++ stack. Only VM::Call (natives calling into the script) and
		// resuming a coroutine nest it. Returns true when the coroutine that it runs yielded instead
		[[nodiscard]] bool Execute(bloop::BloopSize stopDepth);
		// pops the current frame and pushes its return value for the caller. True when that was the last frame to run
		[[nodiscard]] bool ReturnFromFrame(bloop::BloopSize stopDepth);

		// throws when the calls have used more than Limits::m_uStackBytes of the C++ stack
		void CheckNativeStack() const;
		// makes the frame the current one, at the start of its chunk
		void StartFrame(CallFrame& frame);
		void PushFrame(Function* fn);
		void PushFrame(Object* closure);
		void PopFrame();
		void Push(const Value& v);
		[[nodiscard]] Value Pop();

		[[nodiscard]] std::vector<Value> BuildConstants(const std::vector<bloop::ConstantData>& constants);
		[[nodiscard]] ExecutionReturnCode InterpretOpCode(bloop::bytecode::EOpCode op);
		[[nodiscard]] ExecutionReturnCode CallOp();
		// after a native returns: a yield or an exception that it asked for (rc_yield or rc_continue), or nothing
		[[nodiscard]] std::optional<ExecutionReturnCode> HandleNativeSignal();
		// moves a yielding coroutine off the stacks into its state, and back
		void SuspendCoroutine(CoroutineState& state, bloop::BloopSize stackBase, bloop::BloopSize frameBase);
		void RestoreCoroutine(CoroutineState& state, bloop::BloopSize stackBase);
		// (out of line, so the messages' temporaries don't grow the callers' stack frames)
		[[noreturn]] static void ThrowError(const bloop::BloopString& message);
		[[nodiscard]] bloop::BloopIndex FetchOperand();
		// throws unless a script function gets exactly as many arguments as it has parameters
		void CheckArgumentCount(bloop::BloopSize argc, bloop::BloopSize paramCount);
		[[nodiscard]] bloop::BloopOffset FetchTarget(); // of a jump

		[[nodiscard]] std::vector<bc::InstrDebugRef> StackTrace();

		[[nodiscard]] bloop::BloopString FormatStackTraceMessage(const bc::InstrDebugRef& ref);

		void Throw(Value value);
		// unwinds everything after a runtime error and keeps (and maybe prints) the message
		void ReportError(const std::exception& error);

		std::vector<Value> m_oStack;
		std::vector<CallFrame> m_oFrames;
		std::vector<Function> m_oFunctions;
		std::vector<Value> m_oGlobals;
		CallFrame* m_pCurrentFrame{};
		bloop::BloopSize m_uThrowBoundary{}; // a throw never unwinds the frames below this (they belong to a native's caller)
		bloop::BloopBool m_bCoroutineBoundary{}; // ...and they belong to a coroutine's resumer: the exception goes there

		// coroutines
		Object* m_pCoroutine{}; // the running one (null: the main script). Rooted by the resume call's arguments
		// a value on its way out of a native (a yielded value, an exception) or out of a coroutine (GC root)
		Value m_oTransfer;
		enum class NativeSignal : bloop::BloopByte { none, yield, raise } m_eNativeSignal{ NativeSignal::none };
		// VM::Call and coroutine resumes in progress: each one nests the dispatch loop in C++
		bloop::BloopSize m_uCallNesting{};

		const bloop::standard::NativeFunction* m_pCalledNative{};
		std::vector<std::unique_ptr<bloop::BloopString>> m_oHandleTypes; // (a few: a linear search is fine)

		bloop::standard::Environment m_oEnvironment;
		std::chrono::steady_clock::time_point m_oCreationTime{ std::chrono::steady_clock::now() };
		bloop::Random m_oRandom;
		std::optional<bloop::BloopInt> m_oExitCode;
		bool m_bFailed{};
		bool m_bPrintErrors{ true };
		bloop::BloopString m_sError;
		bloop::BloopString m_sErrorMessage;
		std::vector<bc::InstrDebugRef> m_oErrorTrace;
		std::vector<bc::InstrDebugRef> m_oUnwound; // the calls that the exception on its way has left (innermost first)
		Limits m_oLimits;
		bloop::BloopUInt m_uInstructionsUsed{}; // since the host's last Run() / CallFromHost()
		std::uintptr_t m_uStackBase{}; // the C++ stack where the host called Run() / CallFromHost()
		void* m_pUserData{};
		void* m_pEmbedding{};

		Heap m_oHeap;
		GC m_oGC;
		Chunk m_oGlobalChunk; //executed in the beginning
		std::unique_ptr<TaskList> m_pTasks; // (after the GC: its persistent handles go first)

		//debugging
		bloop::metadata::Metadata& m_refMetaData;

		template <typename F>
		inline void BinaryOp(F&& fn) {
			Value b = Pop();
			Value a = Pop();
			Push(fn(a, b));
		}

	};

}
