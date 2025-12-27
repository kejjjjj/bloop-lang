#pragma once
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/heap.hpp"
#include "bytecode/defs.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

#include <ranges>

using namespace bloop::vm;

using TOpCode = bloop::bytecode::EOpCode;

VM::ExecutionReturnCode VM::InterpretOpCode(TOpCode op) {

	switch (op) {
		case TOpCode::LOAD_CONST: {
			Push(m_pCurrentFrame->m_pChunk->m_oConstants[FetchOperand()]);
			break;
		}
		case TOpCode::LOAD_LOCAL: {
			Push(m_oStack[m_pCurrentFrame->m_uBase + FetchOperand()]);
			break;
		}
		case TOpCode::LOAD_GLOBAL: {
			const auto idx = FetchOperand();
			Push(m_oGlobals[idx]);
			break;
		}
		case TOpCode::CREATE_ARRAY: {
			const auto numInitializers = FetchOperand();
			auto arr = m_oHeap.AllocArray(numInitializers);

			for (const auto i : std::views::iota(0u, numInitializers) | std::views::reverse)
				arr->array.values[i] = Pop();

			Push(arr);
			break;
		}
		case TOpCode::DEFINE_GLOBAL: {
			const auto idx = FetchOperand();
			m_oGlobals[idx] = Pop();
			break;
		}
		case TOpCode::STORE_LOCAL: {
			const auto idx = m_pCurrentFrame->m_uBase + FetchOperand();
			assert(idx <= static_cast<bloop::BloopUInt16>(m_oStack.size()));
			m_oStack[idx] = Pop();
			break;
		}
		case TOpCode::STORE_GLOBAL: {
			const auto idx = FetchOperand();
			assert(idx <= static_cast<bloop::BloopUInt16>(m_oStack.size()));
			m_oGlobals[idx] = Pop();
			break;
		}
		case TOpCode::MAKE_FUNCTION: {
			const auto idx = FetchOperand();
			assert(idx <= static_cast<bloop::BloopUInt16>(m_oFunctions.size()));
			Push(m_oHeap.AllocCallable(&m_oFunctions.at(idx)));
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
		case TOpCode::CALL: {
			const auto argc = FetchOperand();

			Value callee = Pop();

			if (!callee.IsCallable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not callable"), callee.TypeToString()));

			if(callee.obj->function->m_uParamCount != argc)
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, callee.obj->function->m_uParamCount));

			RunFunction(callee.obj->function);
			break;
		}
		case TOpCode::SUBSCRIPT_GET: {
			Value index = Pop();
			Value operand = Pop();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			Push(operand.obj->Index(index.ToInt()));
			break;
		}
		case TOpCode::SUBSCRIPT_SET: {
			Value index = Pop();
			Value operand = Pop();
			Value value = Pop();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			operand.obj->Index(index.ToInt()) = value;
			Push(value);
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
	auto& bytecode = m_pCurrentFrame->m_pChunk->m_oByteCode;
	const auto ret = bytecode[m_pCurrentFrame->m_uIp] | (bytecode[m_pCurrentFrame->m_uIp + 1] << 8);
	m_pCurrentFrame->m_uIp += 2; //because operands are 2 bits
	assert(ret <= std::numeric_limits<bloop::BloopUInt16>::max());
	return static_cast<bloop::BloopUInt16>(ret);
}