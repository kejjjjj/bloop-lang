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
	struct CInstructionPosition {
		std::size_t byteOffset;
		CodePosition pos;
	};
	struct Chunk {
		std::vector<Value> m_oConstants;
		std::vector<BloopByte> m_oByteCode;
		std::vector<CInstructionPosition> m_oPositions; //uses the same ip as m_oByteCode
	};
	struct Function {
		Chunk chunk;
		bloop::BloopUInt16 m_uParamCount{};
		bloop::BloopUInt16 m_uLocalCount{};
	};

	struct CallFrame {
		CallFrame(Chunk* fn, std::size_t stackBase) : m_pChunk(fn), m_uBase(stackBase) {}

		[[nodiscard]] const CInstructionPosition& GetCurrentPosition() const {
			auto it = std::upper_bound(m_pChunk->m_oPositions.begin(), m_pChunk->m_oPositions.end(), m_uIp,
				[](std::size_t ip, const CInstructionPosition& p) {
					return ip <= p.byteOffset;
				});
			return *(it - 1);
		}

		Chunk* m_pChunk{};
		std::size_t m_uIp{};
		std::size_t m_uBase{};
		std::size_t m_uCurrentLine{};

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

		inline void PushFrame(Function* fn) {
			const auto frameBase = m_oStack.size() - fn->m_uParamCount;
			m_oStack.resize(frameBase + fn->m_uLocalCount);
			m_pCurrentFrame = &m_oFrames.emplace_back(&fn->chunk, frameBase);
		}
		inline void PopFrame() {
			m_oStack.resize(m_oFrames.back().m_uBase);
			m_oFrames.pop_back();
			m_pCurrentFrame = m_oFrames.empty() ? nullptr : &m_oFrames.back();
		}

		inline void Push(const Value& v) {
			m_oStack.push_back(v);
		}
		[[nodiscard]] inline Value Pop() {
			assert(!m_oStack.empty());
			Value v = m_oStack.back();
			m_oStack.pop_back();
			return v;
		}
		[[nodiscard]] std::vector<Value> BuildConstants(const std::vector<bloop::bytecode::CConstant>& constants);
		[[nodiscard]] ExecutionReturnCode InterpretOpCode(bloop::bytecode::EOpCode op);
		[[nodiscard]] bloop::BloopUInt16 FetchOperand();

		std::vector<Value> m_oStack;
		std::vector<CallFrame> m_oFrames;
		std::vector<Function> m_oFunctions;
		std::unordered_map<bloop::BloopString, Function*> m_oFunctionTable;
		std::vector<Value> m_oGlobals;
		CallFrame* m_pCurrentFrame{};

		Heap m_oHeap;
		GC m_oGC;
		Chunk m_oGlobalChunk; //executed in the beginning
	};

}
