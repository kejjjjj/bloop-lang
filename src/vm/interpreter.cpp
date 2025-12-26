#pragma once
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/heap.hpp"
#include "bytecode/defs.hpp"

#include <iostream>

using namespace bloop::vm;

using TOpCode = bloop::bytecode::EOpCode;

VM::ExecutionReturnCode VM::InterpretOpCode(TOpCode op) {

	switch (op) {
		case TOpCode::LOAD_CONST: {
			Push(m_pCurrentFrame->m_pFunction->m_oConstants[FetchOperand()]);
			break;
		}
		case TOpCode::LOAD_LOCAL: {
			Push(m_oStack[m_pCurrentFrame->m_uBase + FetchOperand()]);
			break;
		}
		case TOpCode::STORE_LOCAL: {
			const auto idx = m_pCurrentFrame->m_uBase + FetchOperand();
			assert(idx <= static_cast<bloop::BloopUInt16>(m_oStack.size()));
			m_oStack[idx] = Pop();
			break;
		}
		case TOpCode::ADD: {
			Value b = Pop();
			Value a = Pop();

			if (a.IsString() && b.IsString()) {
				Push(m_oHeap.StringConcat(a.obj, b.obj));
			} else {
				Push(a + b);
			}
			break;
		}
		case TOpCode::SUB: {
			Value b = Pop();
			Value a = Pop();
			Push(a - b);
			break;
		}
		case TOpCode::MUL: {
			Value b = Pop();
			Value a = Pop();
			Push(a * b);
			break;
		}
		case TOpCode::DIV: {
			Value b = Pop();
			Value a = Pop();
			Push(a / b);
			break;
		}
		case TOpCode::LESS_EQUAL: {
			Value b = Pop();
			Value a = Pop();
			Push(a <= b);
			break;
		}
		case TOpCode::JZ: {
			const auto target = FetchOperand();
			const Value v = Pop();
			if (!v.IsTruthy())
				m_pCurrentFrame->m_uIp = target; // skip to the end of the loop
			break;
		}
		case TOpCode::JMP: {
			m_pCurrentFrame->m_uIp = FetchOperand();
			break;
		}
		case TOpCode::RETURN: {
			return ExecutionReturnCode::rc_return;
		}
		case TOpCode::RETURN_VALUE: {
			return ExecutionReturnCode::rc_return_value;
		}
	}
	return ExecutionReturnCode::rc_continue;
}
#define NOMINMAX
bloop::BloopUInt16 VM::FetchOperand() {
	auto& bytecode = m_pCurrentFrame->m_pFunction->m_oByteCode;
	const auto ret = bytecode[m_pCurrentFrame->m_uIp] | (bytecode[m_pCurrentFrame->m_uIp + 1] << 8);
	m_pCurrentFrame->m_uIp += 2; //because operands are 2 bits
	assert(ret <= std::numeric_limits<bloop::BloopUInt16>::max());
	return static_cast<bloop::BloopUInt16>(ret);
}