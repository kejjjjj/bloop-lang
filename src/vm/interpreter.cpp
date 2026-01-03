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
		} case TOpCode::LOAD_LOCAL: {
			const auto idx = FetchOperand();
			assert(m_pCurrentFrame->m_uBase + idx < m_oStack.size());
			Push(m_oStack[m_pCurrentFrame->m_uBase + idx]);
			break;
		} case TOpCode::LOAD_GLOBAL: {
			const auto idx = FetchOperand();
			Push(m_oGlobals[idx]);
			break;
		} case TOpCode::LOAD_UPVALUE: {
			const auto idx = FetchOperand();
			Push(*m_pCurrentFrame->m_pClosure->upvalues[idx]->location);
			break;
		} case TOpCode::CREATE_ARRAY: {
			const auto numInitializers = FetchOperand();
			auto arr = m_oHeap.AllocArray(numInitializers);

			for (const auto i : std::views::iota(0u, numInitializers) | std::views::reverse)
				arr->array.values[i] = Pop();

			Push(arr);
			break;
		} case TOpCode::STORE_LOCAL: {
			const auto idx = m_pCurrentFrame->m_uBase + FetchOperand();
			assert(idx <= static_cast<bloop::BloopIndex>(m_oStack.size()));
			m_oStack[idx] = Pop();
			break;
		} case TOpCode::STORE_GLOBAL: {
			const auto idx = FetchOperand();
			assert(idx <= static_cast<bloop::BloopIndex>(m_oGlobals.size()));
			m_oGlobals[idx] = Pop();
			break;
		} case TOpCode::STORE_UPVALUE: {
			const auto idx = FetchOperand();
			assert(idx <= static_cast<bloop::BloopIndex>(m_pCurrentFrame->m_pClosure->numValues));
			m_pCurrentFrame->m_pClosure->upvalues[idx]->closed = Pop();
			break;
		} case TOpCode::MAKE_FUNCTION: {
			const auto idx = FetchOperand();
			assert(idx <= static_cast<bloop::BloopIndex>(m_oFunctions.size()));
			Push(m_oHeap.AllocCallable(&m_oFunctions.at(idx)));
			break;
		} case TOpCode::ADD: {
			Value b = Pop();
			Value a = Pop();

			if (a.IsString() && b.IsString()) {
				Push(m_oHeap.StringConcat(a.obj, b.obj));
			} else {
				Push(a + b);
			}
			break;
		} case TOpCode::SUB: {
			Value b = Pop();
			Value a = Pop();
			Push(a - b);
			break;
		} case TOpCode::MUL: {
			Value b = Pop();
			Value a = Pop();
			Push(a * b);
			break;
		} case TOpCode::DIV: {
			Value b = Pop();
			Value a = Pop();
			Push(a / b);
			break;
		} case TOpCode::LESS_EQUAL: {
			Value b = Pop();
			Value a = Pop();
			Push(a <= b);
			break;
		} case TOpCode::JZ: {
			const auto target = FetchOperand();
			const Value v = Pop();
			if (!v.IsTruthy())
				m_pCurrentFrame->m_uIp = target; // skip to the end of the loop
			break;
		} case TOpCode::JMP: {
			m_pCurrentFrame->m_uIp = FetchOperand();
			break;
		} case TOpCode::CALL: {
			const auto argc = FetchOperand();

			Value callee = Pop();

			if (!callee.IsCallable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not callable"), callee.TypeToString()));

			if (callee.obj->type == Object::Type::ot_function) {
				if (callee.obj->function->m_uParamCount != argc)
					throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, callee.obj->function->m_uParamCount));

				RunFunction(callee.obj->function);
			}
			else if (callee.obj->type == Object::Type::ot_closure) {
				if (callee.obj->closure.function->m_uParamCount != argc)
					throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, callee.obj->function->m_uParamCount));

				RunClosure(&callee.obj->closure);
			}
			break;
		} case TOpCode::SUBSCRIPT_GET: {
			Value index = Pop();
			Value operand = Pop();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			Push(operand.obj->Index(index.ToInt()));
			break;
		} case TOpCode::SUBSCRIPT_SET: {
			Value index = Pop();
			Value operand = Pop();
			Value value = Pop();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			operand.obj->Index(index.ToInt()) = value;
			Push(value);
			break;
		} case TOpCode::RETURN: {
			return ExecutionReturnCode::rc_return;
		} case TOpCode::RETURN_VALUE: {
			return ExecutionReturnCode::rc_return_value;
		} case TOpCode::MAKE_CLOSURE: {
			const auto funcIdx = FetchOperand();
			assert(funcIdx < static_cast<bloop::BloopIndex>(m_oFunctions.size()));
			auto& func = m_oFunctions[funcIdx];

			auto obj = m_oHeap.AllocClosure(&func, func.m_oCaptures.size());

			for (const auto i : std::views::iota(0u, obj->closure.numValues)) {
				const auto opcode = static_cast<TOpCode>(m_pCurrentFrame->m_pChunk->m_oByteCode[m_pCurrentFrame->m_uIp++]);
				const auto slot = FetchOperand();

				if (opcode == TOpCode::CAPTURE_LOCAL)
					obj->closure.upvalues[i] = CaptureUpValue(&m_oStack[m_pCurrentFrame->m_uBase + slot]);
				else
					obj->closure.upvalues[i] = m_pCurrentFrame->m_pClosure->upvalues[slot];
			}
			Push(obj);
			break;
		}
	}
	return ExecutionReturnCode::rc_continue;
}
#define NOMINMAX
bloop::BloopIndex VM::FetchOperand() {
	auto* p = &m_pCurrentFrame->m_pChunk->m_oByteCode[m_pCurrentFrame->m_uIp];

	bloop::BloopIndex value;
	std::memcpy(&value, p, sizeof(value));

	m_pCurrentFrame->m_uIp += sizeof(value);
	return value;
}