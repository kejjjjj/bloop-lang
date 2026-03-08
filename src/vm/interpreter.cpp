#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/heap.hpp"
#include "bytecode/defs.hpp"
#include "vm/exception.hpp"
#include "std/native.hpp"
#include "utils/fmt.hpp"
#include "vm/frame.hpp"

#include <ranges>
#include <cstring>

using namespace bloop::vm;

using TOpCode = bloop::bytecode::EOpCode;



VM::ExecutionReturnCode VM::InterpretOpCode(TOpCode op) {

	switch (op) {
		case TOpCode::POP: {
			assert(!m_oStack.empty());
			[[maybe_unused]] const auto _ = Pop();
			break;
		} case TOpCode::LOAD_CONST: {
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
		} case TOpCode::CREATE_OBJECT: {
			const auto numInitializers = FetchOperand();
			auto obj = m_oHeap.AllocObject(numInitializers);

			for ([[maybe_unused]] const auto i : std::views::iota(0u, numInitializers)) {
				auto v = Pop();
				auto k = Pop();
				obj->ObjectSet(k, v);
			}

			Push(obj);
			break;
		} case TOpCode::STORE_LOCAL: {
			const auto idx = m_pCurrentFrame->m_uBase + FetchOperand();
			assert(idx < static_cast<bloop::BloopIndex>(m_oStack.size()));
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
			*m_pCurrentFrame->m_pClosure->upvalues[idx]->location = Pop();
			break;
		} case TOpCode::MAKE_FUNCTION: {
			const auto idx = FetchOperand();
			assert(idx <= static_cast<bloop::BloopIndex>(m_oFunctions.size()));
			Push(m_oHeap.AllocCallable(&m_oFunctions.at(idx)));
			break;
		} case TOpCode::ADD:
			BinaryOp([&](auto& a, auto& b) {
				if (a.IsString() && b.IsString())
					return Value(m_oHeap.StringConcat(a.obj, b.obj));
				return a + b;
			});
			break;
		case TOpCode::SUB:			BinaryOp([](auto& a, auto& b) { return a - b; }); break;
		case TOpCode::MUL:			BinaryOp([](auto& a, auto& b) { return a * b; }); break;
		case TOpCode::DIV:			BinaryOp([](auto& a, auto& b) { return a / b; }); break;
		case TOpCode::MOD:			BinaryOp([](auto& a, auto& b) { return a % b; }); break;
		case TOpCode::LESS:			BinaryOp([](auto& a, auto& b) { return a < b; }); break;
		case TOpCode::LESS_EQUAL:	BinaryOp([](auto& a, auto& b) { return a <= b; }); break;
		case TOpCode::GREATER:		BinaryOp([](auto& a, auto& b) { return a > b; }); break;
		case TOpCode::GREATER_EQUAL:BinaryOp([](auto& a, auto& b) { return a >= b; }); break;
		case TOpCode::LOGICAL_OR:	BinaryOp([](auto& a, auto& b) { return a || b; }); break;
		case TOpCode::LOGICAL_AND:	BinaryOp([](auto& a, auto& b) { return a && b; }); break;
		case TOpCode::EQ:			BinaryOp([](auto& a, auto& b) { return a == b; }); break;
		case TOpCode::NE:			BinaryOp([](auto& a, auto& b) { return a != b; }); break;
		case TOpCode::S_EQ:			BinaryOp([](auto& a, auto& b) { return (a.type == b.type) && a.IsEqual(b); }); break;
		case TOpCode::S_NE:			BinaryOp([](auto& a, auto& b) { return (a.type != b.type) || !a.IsEqual(b); }); break;
		case TOpCode::SEQUENCE:		BinaryOp([]([[maybe_unused]]auto&, auto& b) { return b; }); break; 
		case TOpCode::LEFT_SHIFT:	BinaryOp([](auto& a, auto& b) { return a << b; }); break;
		case TOpCode::RIGHT_SHIFT:	BinaryOp([](auto& a, auto& b) { return a >> b; }); break;
		case TOpCode::BITWISE_OR:	BinaryOp([](auto& a, auto& b) { return a | b; }); break;
		case TOpCode::BITWISE_AND:	BinaryOp([](auto& a, auto& b) { return a & b; }); break;
		case TOpCode::BITWISE_XOR:	BinaryOp([](auto& a, auto& b) { return a ^ b; }); break;

		case TOpCode::JZ: {
			const auto target = FetchOperand();
			const Value v = Pop();
			if (!v.IsTruthy())
				m_pCurrentFrame->m_pIp = m_pCurrentFrame->m_pIpBase + target; // skip to the end of the loop
			break;
		} case TOpCode::JMP: {
			m_pCurrentFrame->m_pIp = m_pCurrentFrame->m_pIpBase + FetchOperand();
			break;
		} case TOpCode::CALL: {
			const auto argc = FetchOperand();

			Value callee = Pop();

			if (!callee.IsCallable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not callable"), callee.TypeToString()));

			switch (callee.obj->type) {
			case Object::Type::ot_function:
				if (callee.obj->nativeFunction->m_uParamCount != bloop::VARIADIC_PARAMETER_COUNT && callee.obj->function->m_uParamCount != argc)
					throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, callee.obj->function->m_uParamCount));

				RunFunction(callee.obj->function);
				break;
				
			case Object::Type::ot_closure:
				if (callee.obj->nativeFunction->m_uParamCount != bloop::VARIADIC_PARAMETER_COUNT && callee.obj->closure.function->m_uParamCount != argc)
					throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, callee.obj->closure.function->m_uParamCount));

				RunClosure(&callee.obj->closure);
				break;
			case Object::Type::ot_nativefunction: {
					if (callee.obj->nativeFunction->m_uParamCount != bloop::VARIADIC_PARAMETER_COUNT && callee.obj->nativeFunction->m_uParamCount != argc)
						throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, callee.obj->nativeFunction->m_uParamCount));

					std::vector<Value> args(argc);
					for (auto i = argc; i-- > 0; )
						args[i] = Pop();

					Push(callee.obj->nativeFunction->m_pFunction(*this, args));
					break;
				}
			default:
				assert(false);
				break;
			}
			break;
		} case TOpCode::SUBSCRIPT_GET: {
			Value index = Pop();
			Value operand = Pop();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			if (operand.IsString()) {
				auto c = operand.obj->IndexChar(index.ToInt());
				Push(m_oHeap.AllocString(&c, sizeof(c)));
			} else {
				Push(operand.obj->Index(index));
			}
			break;
		} case TOpCode::SUBSCRIPT_SET: {
			Value index = Pop();
			Value operand = Pop();
			Value value = Pop();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			//returns Value&
			auto& idx = operand.obj->Index(index);

			if (value.type == Value::Type::t_object && idx.type == Value::Type::t_object)
				m_oGC.m_oMinorSpace.WriteBarrier(operand.obj, &idx.obj, value.obj);

			idx = value;
			Push(value);
			break;
		} case TOpCode::PROPERTY_GET: {
			Value key = Pop();
			Value operand = Pop();

			if (!operand.IsAggregate())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not an aggregate type"), operand.TypeToString()));

			Push(operand.obj->ObjectGet(key));
			break;

		} case TOpCode::PROPERTY_SET: {
			Value key = Pop();
			Value operand = Pop();
			Value value = Pop();

			if (!operand.IsAggregate())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not an aggregate type"), operand.TypeToString()));

			if (value.type == Value::Type::t_object)
				m_oGC.m_oMinorSpace.WriteBarrier(operand.obj, &key.obj, value.obj);

			operand.obj->ObjectSet(key, value);
			Push(value);
			break;

		} case TOpCode::RETURN: {
			return ExecutionReturnCode::rc_return;
		} case TOpCode::RETURN_VALUE: {
			return ExecutionReturnCode::rc_return_value;
		} case TOpCode::MAKE_CLOSURE: {
			const auto funcIdx = FetchOperand();

			assert(funcIdx < static_cast<bloop::BloopIndex>(m_oFunctions.size()) && "Function index out of bounds");
			assert(funcIdx < 10000 && "Suspiciously large function index - possible bytecode corruption");

			auto& func = m_oFunctions[funcIdx];

			assert(func.m_oCaptures.size() <= 1024 && "Unreasonably large number of captures in function");
			assert(!func.m_oCaptures.empty() || (func.m_oCaptures.size() == 0 && "Function capture metadata inconsistent"));

			auto obj = m_oHeap.AllocClosure(&func, static_cast<bloop::BloopIndex>(func.m_oCaptures.size()));

			assert(obj != nullptr && "AllocClosure returned null");
			assert(obj->type == Object::Type::ot_closure && "Allocated object is not a closure");
			assert(GCHeader::GetHeader(obj) != nullptr && "Allocated closure has no GC header");
			assert(obj->closure.numValues == func.m_oCaptures.size() && "Closure upvalue count doesn't match function captures");
			assert(obj->closure.upvalues != nullptr || (obj->closure.numValues == 0 && "Closure has null upvalues array but numValues > 0"));

			m_oGC.PushTempRoot(obj);

			for (const auto i : std::views::iota(0u, obj->closure.numValues)) {
				[[maybe_unused]] const auto& chunk = m_refMetaData.m_oVMData.m_oChunks[m_pCurrentFrame->m_pChunk->m_uMetadata];

				assert(m_pCurrentFrame->m_pChunk->m_uMetadata < m_refMetaData.m_oVMData.m_oChunks.size() && "Current chunk metadata index out of bounds");

				assert(m_pCurrentFrame->m_pIp < chunk.m_oByteCode.data() + chunk.m_oByteCode.size() && "IP out of bounds before reading opcode");
				const auto opcode = static_cast<TOpCode>(*m_pCurrentFrame->m_pIp++);
				assert(m_pCurrentFrame->m_pIp < chunk.m_oByteCode.data() + chunk.m_oByteCode.size() && "IP advanced past end after opcode read");

				const auto slot = FetchOperand();

				if (opcode == TOpCode::CAPTURE_LOCAL) {

					const auto stack_idx = m_pCurrentFrame->m_uBase + slot;
					assert(stack_idx < m_oStack.size() && "Capture local: stack index out of bounds");
					assert(&m_oStack[stack_idx] >= m_oStack.data() && &m_oStack[stack_idx] < m_oStack.data() + m_oStack.size()
						&& "Capture local: computed pointer outside stack");

					auto* slot_ptr = &m_oStack[stack_idx];
					obj->closure.upvalues[i] = m_oGC.CaptureUpValue(slot_ptr);

					assert(obj->closure.upvalues[i] != nullptr && "CaptureUpValue returned null upvalue");
					assert(obj->closure.upvalues[i]->location == slot_ptr
						&& "Captured upvalue location doesn't match requested stack slot");

					obj->closure.upvalues[i] = m_oGC.CaptureUpValue(&m_oStack[m_pCurrentFrame->m_uBase + slot]);
				} else {

					assert(m_pCurrentFrame->m_pClosure != nullptr && "CAPTURE_UPVALUE but current frame has no closure");
					assert(slot < m_pCurrentFrame->m_pClosure->numValues && "Parent upvalue index out of bounds");
					assert(m_pCurrentFrame->m_pClosure->upvalues != nullptr && "Parent closure has null upvalues array");

					obj->closure.upvalues[i] = m_pCurrentFrame->m_pClosure->upvalues[slot];
				}
			}
			Push(obj);
			m_oGC.PopTempRoot();
			break;
		} case TOpCode::TRY: {
			//ip, catchvar
			m_pCurrentFrame->m_oExceptionHandlers.push_back({ FetchOperand(), m_oStack.size(), FetchOperand() });
			break;
		}case TOpCode::TRY_END: {
			assert(!m_pCurrentFrame->m_oExceptionHandlers.empty());
			m_pCurrentFrame->m_oExceptionHandlers.pop_back();
			break;
		} case TOpCode::THROW: {
			const auto wasEmpty = m_pCurrentFrame->m_oExceptionHandlers.empty();
			Throw(Pop());
			if(wasEmpty) // we are in a different frame, so exit it
				return ExecutionReturnCode::rc_throw;
			break;
		} case TOpCode::DUP: {
			assert(!m_oStack.empty());
			Push(m_oStack.back());
			break;
		} case TOpCode::INCR: {
			Value index = Pop();
			Push(++index);
			break;
		} case TOpCode::DECR: {
			Value index = Pop();
			Push(--index);
			break;
		} case TOpCode::NEG: {
			Value index = Pop();
			Push(-index);
			break;
		} default: {
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("unknown opcode?")));
		}
	}
	return ExecutionReturnCode::rc_continue;
}
#define NOMINMAX
bloop::BloopIndex VM::FetchOperand() {
	auto* p = &*m_pCurrentFrame->m_pIp;

	bloop::BloopIndex value;
	std::memcpy(&value, p, sizeof(value));

	m_pCurrentFrame->m_pIp += sizeof(value);
	return value;
}