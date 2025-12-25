#pragma once

#include <vector>
#include <unordered_map>
#include <cassert>

#include "value.hpp"


namespace bloop::bytecode {
	enum class EOpCode : unsigned char;
	namespace vmdata {
		struct Function;
	}
}

namespace bloop::vm
{
	struct Function {
		bloop::BloopUInt16 m_uParamCount{};
		bloop::BloopUInt16 m_uLocalCount{};
		std::vector<Value> m_oConstants;
		std::vector<BloopByte> m_oByteCode;
	};

	struct CallFrame {
		Function* m_pFunction{};
		std::size_t m_uIp{};
		std::size_t m_uBase{};

		CallFrame(Function* fn, std::size_t stackBase) : m_pFunction(fn), m_uBase(stackBase){}
	};

	class VM {
	public:
		VM(const std::vector<bloop::bytecode::vmdata::Function>& funcs);

		void Run(const bloop::BloopString& entryFuncName);
		void RunFunction(Function* fn);

		inline void PushFrame(Function* fn) {
			m_pCurrentFrame = &m_oFrames.emplace_back(fn, m_oStack.size());
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
	private:
		[[nodiscard]] bool InterpretOpCode(bloop::bytecode::EOpCode op);
		[[nodiscard]] bloop::BloopUInt16 FetchOperand();

		std::vector<Value> m_oStack;
		std::vector<CallFrame> m_oFrames;
		std::vector<Function> m_oFunctions;
		std::unordered_map<bloop::BloopString, Function*> m_oFunctionTable;
		CallFrame* m_pCurrentFrame{};
	};

}
