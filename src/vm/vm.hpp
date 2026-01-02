#pragma once

#include <vector>
#include <unordered_map>
#include <cassert>

#include "vm/value.hpp"
#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"

namespace bloop::bytecode {
	enum class EOpCode : unsigned char;
	struct CConstant;
	struct VMByteCode;
	namespace vmdata {
		struct Function;
	}
}

namespace bloop::vm
{
	struct Closure;
	struct Capture {
		bloop::BloopIndex m_uSlot{};
		bool m_bIsLocal;
	};
	struct CInstructionPosition {
		bloop::BloopIndex byteOffset;
		CodePosition pos;
	};
	struct Chunk {
		std::vector<Value> m_oConstants;
		std::vector<BloopByte> m_oByteCode;
		std::vector<CInstructionPosition> m_oPositions; //uses the same ip as m_oByteCode
	};
	struct Function {
		Chunk chunk;
		bloop::BloopIndex m_uParamCount{};
		bloop::BloopIndex m_uLocalCount{};
		std::vector<Capture> m_oCaptures{};
	};

	struct CallFrame {
		CallFrame(Chunk* fn, std::size_t stackBase);
		CallFrame(Closure* closure, std::size_t stackBase);

		[[nodiscard]] const CInstructionPosition& GetCurrentPosition() const;

		Closure* m_pClosure{};
		Chunk* m_pChunk{};
		std::size_t m_uIp{};
		std::size_t m_uBase{};

	};

	class VM {
		friend class GC;
		friend class Heap;
	public:
		VM(const bloop::bytecode::VMByteCode& bc);
		~VM();

		void Run(const bloop::BloopString& entryFuncName);

	private:
		enum class ExecutionReturnCode : bloop::BloopByte {
			rc_continue,
			rc_return,
			rc_return_value
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

		[[nodiscard]] std::vector<Value> BuildConstants(const std::vector<bloop::bytecode::CConstant>& constants);
		[[nodiscard]] ExecutionReturnCode InterpretOpCode(bloop::bytecode::EOpCode op);
		[[nodiscard]] bloop::BloopIndex FetchOperand();

		UpValue* CaptureUpValue(Value* slot);
		void CloseUpValues(Value* lastSlot);

		std::vector<Value> m_oStack;
		std::vector<CallFrame> m_oFrames;
		std::vector<Function> m_oFunctions;
		std::unordered_map<bloop::BloopString, Function*> m_oFunctionTable;
		std::vector<Value> m_oGlobals;
		CallFrame* m_pCurrentFrame{};

		Heap m_oHeap;
		GC m_oGC;
		Chunk m_oGlobalChunk; //executed in the beginning

		UpValue* m_pOpenUpValues{};
	};

}
