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

namespace bloop::bytecode {
	enum class EOpCode : unsigned char;
}

namespace bloop::vm {
	struct CallFrame;
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
			rc_return_value,
			rc_throw
		};

		[[nodiscard]] ExecutionReturnCode RunFrame();
		void RunGlobal();
		void RunFunction(Function* fn);
		void RunClosure(Object* closure);

		void PushFrame(Function* fn);
		void PushFrame(Object* closure);
		void PopFrame();
		void Push(const Value& v);
		[[nodiscard]] Value Pop();

		[[nodiscard]] std::vector<Value> BuildConstants(const std::vector<bloop::ConstantData>& constants);
		[[nodiscard]] ExecutionReturnCode InterpretOpCode(bloop::bytecode::EOpCode op);
		[[nodiscard]] bloop::BloopIndex FetchOperand();

		[[nodiscard]] std::vector<bc::InstrDebugRef> StackTrace();

		[[nodiscard]] bloop::BloopString FormatStackTraceMessage(const bc::InstrDebugRef& ref);

		void Throw(Value value);

		std::vector<Value> m_oStack;
		std::vector<CallFrame> m_oFrames;
		std::vector<Function> m_oFunctions;
		std::vector<Value> m_oGlobals;
		CallFrame* m_pCurrentFrame{};
		bloop::BloopSize m_uThrowBoundary{}; // a throw never unwinds the frames below this (they belong to a native's caller)

		bloop::standard::Environment m_oEnvironment;
		std::optional<bloop::BloopInt> m_oExitCode;
		bool m_bFailed{};

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
