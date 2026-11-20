#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/heap.hpp"
#include "bytecode/defs.hpp"
#include "vm/exception.hpp"
#include "std/native.hpp"
#include "utils/fmt.hpp"
#include "vm/frame.hpp"
#include "vm/gc/handles.hpp"

#include <ranges>
#include <cstring>
#include <span>

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

			m_oGC.WriteBarrierAll(arr); // big arrays are allocated straight into the old generation
			Push(arr);
			break;
		} case TOpCode::CREATE_OBJECT: {
			const auto numInitializers = FetchOperand();
			auto obj = m_oHeap.AllocObject(numInitializers);

			// insert in source order, so that the keys keep the order they were written in
			const auto base = m_oStack.size() - 2u * numInitializers;
			for (const auto i : std::views::iota(0u, static_cast<bloop::BloopSize>(numInitializers)))
				obj->ObjectSet(m_oStack[base + 2u * i], m_oStack[base + 2u * i + 1u]);
			m_oStack.resize(base);

			m_oGC.WriteBarrierAll(obj);
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
			m_oGC.WriteUpValue(m_pCurrentFrame->m_pClosure->upvalues[idx], Pop());
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
			const auto target = FetchTarget();
			const Value v = Pop();
			if (!v.IsTruthy())
				m_pCurrentFrame->m_pIp = m_pCurrentFrame->m_pIpBase + target; // skip to the end of the loop
			break;
		} case TOpCode::JMP: {
			m_pCurrentFrame->m_pIp = m_pCurrentFrame->m_pIpBase + FetchTarget();
			break;
		} case TOpCode::CALL: {
			return CallOp(); // (normally Execute calls it directly)
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
			// [value, operand, index] -> [value]. The value stays on the stack (a root) while the object may grow
			Value index = Pop();
			Value operand = Pop();
			const auto& value = m_oStack.back();

			if (!operand.IsIndexable())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not indexable"), operand.TypeToString()));

			// obj["key"] = value adds the key when it's missing, like obj.key = value
			if (operand.obj->IsObject()) {
				if (!index.IsString())
					throw exception::VMError(BLOOPTEXT("object index must be a string"));

				m_oHeap.ObjectSet(operand.obj, index, value);
				break;
			}

			//returns Value&
			auto& idx = operand.obj->Index(index);

			idx = value;
			m_oGC.WriteBarrier(operand.obj, value);
			break;
		} case TOpCode::PROPERTY_GET: {
			Value key = Pop();
			Value operand = Pop();

			if (!operand.IsAggregate())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not an aggregate type"), operand.TypeToString()));

			Push(operand.obj->ObjectGet(key));
			break;

		} case TOpCode::PROPERTY_SET: {
			// [value, operand, key] -> [value]. The value stays on the stack (a root) while the object may grow
			Value key = Pop();
			Value operand = Pop();

			if (!operand.IsAggregate())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not an aggregate type"), operand.TypeToString()));

			m_oHeap.ObjectSet(operand.obj, key, m_oStack.back());
			break;

		} case TOpCode::RETURN: {
			return ExecutionReturnCode::rc_return;
		} case TOpCode::RETURN_VALUE: {
			return ExecutionReturnCode::rc_return_value;
		} case TOpCode::MAKE_CLOSURE: {
			const auto funcIdx = FetchOperand();

			assert(funcIdx < static_cast<bloop::BloopIndex>(m_oFunctions.size()) && "Function index out of bounds");

			auto& func = m_oFunctions[funcIdx];
			const auto numCaptures = static_cast<bloop::BloopIndex>(func.m_oCaptures.size());

			[[maybe_unused]] const auto& chunk = m_refMetaData.m_oVMData.m_oChunks[m_pCurrentFrame->m_pChunk->m_uMetadata];

			// capturing allocates upvalues, which can move the closure
			RootScope scope(m_oGC);
			const auto closure = scope.Keep(m_oHeap.AllocClosure(&func, numCaptures));

			for (const auto i : std::views::iota(0u, static_cast<bloop::BloopUInt>(numCaptures))) {
				assert(m_pCurrentFrame->m_pIp < chunk.m_oByteCode.data() + chunk.m_oByteCode.size() && "IP out of bounds before reading opcode");
				const auto opcode = static_cast<TOpCode>(*m_pCurrentFrame->m_pIp++);
				const auto slot = FetchOperand();

				UpValue* uv{};

				if (opcode == TOpCode::CAPTURE_GLOBAL) {
					assert(slot < m_oGlobals.size());
					uv = m_oGC.NewClosedUpValue(&m_oGlobals[slot]);
				} else if (opcode == TOpCode::CAPTURE_LOCAL) {
					const auto stack_idx = m_pCurrentFrame->m_uBase + slot;
					assert(stack_idx < m_oStack.size() && "Capture local: stack index out of bounds");

					uv = m_oGC.CaptureUpValue(&m_oStack[stack_idx]);
					assert(uv && uv->location == &m_oStack[stack_idx] && "Captured upvalue location doesn't match requested stack slot");
				} else {
					assert(opcode == TOpCode::CAPTURE_UPVALUE);
					assert(m_pCurrentFrame->m_pClosure != nullptr && "CAPTURE_UPVALUE but current frame has no closure");
					assert(slot < m_pCurrentFrame->m_pClosure->numValues && "Parent upvalue index out of bounds");

					uv = m_pCurrentFrame->m_pClosure->upvalues[slot];
				}

				// re-read: the closure may have moved. Upvalues are always old, so no write barrier is needed
				closure->closure.upvalues[i] = uv;
			}

			Push(closure.Get());
			break;
		} case TOpCode::TRY: {
			//ip, catchvar
			const auto target = FetchTarget();
			const auto catchVar = FetchOperand();
			m_pCurrentFrame->m_oExceptionHandlers.push_back({ target, m_oStack.size(), catchVar });
			break;
		}case TOpCode::TRY_END: {
			assert(!m_pCurrentFrame->m_oExceptionHandlers.empty());
			m_pCurrentFrame->m_oExceptionHandlers.pop_back();
			break;
		} case TOpCode::THROW: {
			// unwinds to the frame with the closest handler and jumps to its catch, which is where Execute carries on
			// (or throws a VMError when nothing catches it)
			Throw(Pop());
			break;
		} case TOpCode::ITER_PREPARE: {
			assert(!m_oStack.empty());
			const auto& iterable = m_oStack.back();

			if (!iterable.IsArray() && !iterable.IsString())
				throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not iterable"), iterable.TypeToString()));

			Push(Value{ bloop::BloopInt{ 0 } });
			break;
		} case TOpCode::ITER_NEXT: {
			const auto target = FetchTarget();
			assert(m_oStack.size() >= 2u);

			const auto index = m_oStack[m_oStack.size() - 1u].i;
			const auto* iterable = m_oStack[m_oStack.size() - 2u].obj;

			// the length is read on every iteration (like js)
			const auto length = iterable->IsArray() ? iterable->array.count : iterable->string.len;

			if (index >= length) {
				m_pCurrentFrame->m_pIp = m_pCurrentFrame->m_pIpBase + target;
				break;
			}

			m_oStack[m_oStack.size() - 1u].i++;

			if (iterable->IsArray()) {
				Push(iterable->array.values[index]);
			} else {
				const auto c = iterable->string.data[index];
				Push(m_oHeap.AllocString(&c, sizeof(c))); // may move the iterable, which is fine: it's on the stack
			}
			break;
		} case TOpCode::CLOSE_UPVALUES: {
			// gives closures created in a loop body their own copy of the loop's variables
			const auto slot = m_pCurrentFrame->m_uBase + FetchOperand();
			assert(slot < m_oStack.size());
			m_oGC.CloseUpValues(&m_oStack[slot]);
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
		} case TOpCode::NOT: {
			const Value value = Pop();
			Push(Value{ static_cast<bloop::BloopBool>(!value.IsTruthy()) });
			break;
		} case TOpCode::BITWISE_NOT: {
			Value value = Pop();
			Push(~value);
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
// a call to a script function only pushes its frame: Execute carries on in it, and RETURN comes back here.
// Natives are called right away
VM::ExecutionReturnCode VM::CallOp() {
	const auto argc = FetchOperand();

	Value callee = Pop();

	if (!callee.IsCallable())
		ThrowError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not callable"), callee.TypeToString()));

	switch (callee.obj->type) {
	case Object::Type::ot_function:
		CheckArgumentCount(argc, callee.obj->function->m_uParamCount);
		PushFrame(callee.obj->function);
		break;

	case Object::Type::ot_closure:
		CheckArgumentCount(argc, callee.obj->closure.function->m_uParamCount);

		// the frame roots the closure object for the duration of the call
		PushFrame(callee.obj);
		break;
	case Object::Type::ot_nativefunction: {
		const auto* native = callee.obj->nativeFunction;
		if (native->m_uParamCount != bloop::VARIADIC_PARAMETER_COUNT)
			CheckArgumentCount(argc, native->m_uParamCount);

		// the arguments stay on the stack during the call, so they remain GC roots
		// (and get updated in place if the native allocates). The stack never reallocates.
		assert(m_oStack.size() >= argc);
		const auto base = m_oStack.size() - argc;
		const auto result = native->m_pFunction(*this, std::span<const Value>(m_oStack.data() + base, argc));

		m_oStack.resize(base);
		Push(result);
		break;
	}
	default:
		assert(false);
		break;
	}
	return ExecutionReturnCode::rc_continue;
}

void VM::ThrowError(const bloop::BloopString& message) {
	throw exception::VMError(message);
}

bloop::BloopOffset VM::FetchTarget() {
	bloop::BloopOffset value;
	std::memcpy(&value, &*m_pCurrentFrame->m_pIp, sizeof(value));
	m_pCurrentFrame->m_pIp += sizeof(value);
	return value;
}

bloop::BloopIndex VM::FetchOperand() {
	auto* p = &*m_pCurrentFrame->m_pIp;

	bloop::BloopIndex value;
	std::memcpy(&value, p, sizeof(value));

	m_pCurrentFrame->m_pIp += sizeof(value);
	return value;
}