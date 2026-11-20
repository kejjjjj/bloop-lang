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

#include <optional>
#include <exception>
#include <cstdint>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace bloop::bytecode {
	enum class EOpCode : unsigned char;
}

namespace bloop::vm {
	struct CallFrame;

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

		// ends the script from a native function (throws, Run() returns undefined and GetExitCode() has the code)
		void Exit(bloop::BloopInt code);

		// ---- embedding (see bloop.hpp, which wraps all of this) ----

		void SetLimits(const Limits& limits);
		[[nodiscard]] const Limits& GetLimits() const noexcept { return m_oLimits; }

		// runtime errors are printed to std::cout by default. A host can turn that off and read GetError() instead
		void SetPrintErrors(bool print) noexcept { m_bPrintErrors = print; }
		// the message of the last runtime error (with the stack trace)
		[[nodiscard]] const bloop::BloopString& GetError() const noexcept { return m_sError; }

		// calls a script function from the host after Run(): runtime errors and std.process.exit end the call
		// (not the host). Returns nothing then, see Failed(), GetError() and GetExitCode()
		[[nodiscard]] std::optional<Value> CallFromHost(const Value& callee, std::span<const Value> args);

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

	private:
		enum class ExecutionReturnCode : bloop::BloopByte {
			rc_continue,
			rc_return,
			rc_return_value
		};

		void RunGlobal();
		// runs the frames until the frame count drops back to stopDepth, which leaves the return value of
		// the last one on the stack. Script functions calling each other stay in this loop (CALL pushes a frame,
		// RETURN pops it), so they don't use the C++ stack. Only VM::Call (natives calling into the script) nests it
		void Execute(bloop::BloopSize stopDepth);
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

		bloop::standard::Environment m_oEnvironment;
		std::optional<bloop::BloopInt> m_oExitCode;
		bool m_bFailed{};
		bool m_bPrintErrors{ true };
		bloop::BloopString m_sError;
		Limits m_oLimits;
		bloop::BloopUInt m_uInstructionsUsed{}; // since the host's last Run() / CallFromHost()
		std::uintptr_t m_uStackBase{}; // the C++ stack where the host called Run() / CallFromHost()
		void* m_pUserData{};
		void* m_pEmbedding{};

		Heap m_oHeap;
		GC m_oGC;
		Chunk m_oGlobalChunk; //executed in the beginning

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
