#pragma once

#include <vector>
#include <unordered_map>
#include <cassert>

#include "vm/vm_defs.hpp"

#include "metadata/metadata.hpp"
#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "vm/heap/arena.hpp"

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
		VM(bloop::metadata::Metadata& metadata);
		~VM();

		[[maybe_unused]] Value Run();

		void RunGC() { m_oGC.Collect(); }

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
		void RunClosure(Closure* closure);

		void PushFrame(Function* fn);
		void PushFrame(Closure* fn);
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

		//Arena m_oArena;
		GC m_oGC;
		Heap m_oHeap;
		Chunk m_oGlobalChunk; //executed in the beginning

		//debugging
		bloop::metadata::Metadata& m_refMetaData;

	};

}
