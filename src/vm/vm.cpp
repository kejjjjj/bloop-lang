#include "bytecode/defs.hpp"
#include "bytecode/function/bc_function.hpp"
#include "std/vm/to_vm.hpp"
#include "utils/fmt.hpp"
#include "vm.hpp"
#include "vm/exception.hpp"
#include "vm/frame.hpp"
#include "vm/heap/dvalue.hpp"

#include <cassert>
#include <ranges>
#include <utility>

using namespace bloop::vm;

std::vector<Value> VM::BuildConstants(const std::vector<bloop::ConstantData>& constants) {

	// constants live as long as the VM, so they go straight to the old generation.
	// `vals` isn't a GC root yet, so nothing may get collected while it's being built
	GC::PretenureScope pretenure(m_oGC);

	std::vector<Value> vals;
	vals.reserve(constants.size());

	for (const auto& c : constants) {
		const auto& data = std::get<0>(c);
		const auto type = std::get<1>(c);

		if (type == bloop::EValueType::t_string) {
			vals.emplace_back(Value{ m_oHeap.AllocString(data.data(), data.size()) });
		} else {
			vals.emplace_back(Value{ c });
		}
	}
	return vals;
}
VM::VM(bloop::metadata::Metadata& metadata, bloop::standard::Environment environment)
	: m_oEnvironment(std::move(environment)), m_oHeap(&m_oGC), m_oGC(this), m_refMetaData(metadata) {
	
	assert(metadata.m_oVMData.m_pGlobalChunk);
	m_oGlobalChunk.m_oConstants = BuildConstants(metadata.m_oVMData.m_pGlobalChunk->m_oConstants);
	m_oGlobalChunk.m_uMetadata = metadata.m_oVMData.m_pGlobalChunk->m_uId;

	m_oGlobals.resize(metadata.m_uNumGlobals);
	m_oFunctions.reserve(metadata.m_oVMData.m_oFunctions.size());

	for (const auto& metadataFunction : metadata.m_oVMData.m_oFunctions) {
		const auto& chunk = metadata.m_oVMData.m_oChunks[metadataFunction.m_uChunkIndex];

		Function f;
		f.chunk = { chunk.m_uId, BuildConstants(chunk.m_oConstants) };
		f.m_uLocalCount = metadataFunction.m_uLocalCount;
		f.m_uParamCount = metadataFunction.m_uParamCount;
		f.m_uId = metadataFunction.m_uId;
		f.m_oCaptures = metadataFunction.m_oCaptures;
		f.m_uChunkIndex = chunk.m_uId;

		m_oFunctions.emplace_back(f);
	}

	m_oStack.reserve(BLOOP_MAX_STACK);
	m_oFrames.reserve(BLOOP_MAX_FRAMES);
}
// the GC owns all of the memory (nursery + old generation chunks) and releases it wholesale
VM::~VM() = default;

#include <chrono>
#include <iostream>
#include <iomanip>
template<typename Callable>
void Benchmark(const char* name, Callable&& fn) {
	using clock = std::chrono::high_resolution_clock;
	const auto start = clock::now();
	fn();
	const auto end = clock::now();

	const auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
	//const auto ns = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cout << name << ": " << std::setprecision(6) << seconds << "s\n";
}

Value VM::Run() {

	//const auto entryFuncName = BLOOPTEXT("main");
	//bloop::vm::Function* func{ nullptr };
	//if (m_refMetaData.m_oFunctionTable.contains(entryFuncName)) {
	//	const auto idx = m_refMetaData.m_oFunctionTable.at(entryFuncName)->m_uId;
	//	func = &m_oFunctions[idx];
	//	//throw exception::VMError(BLOOPTEXT("couldn't find the entry function: " + entryFuncName));
	//}

	try {
		if (m_refMetaData.m_bDebugOutput)
			Benchmark("glob+main", [&]() { RunGlobal(); });
		else
			RunGlobal();

	} catch (exception::ExitRequest& exit) {
		// unwind everything (closing the upvalues) like an error would, but quietly
		[[maybe_unused]] const auto positions = StackTrace();
		m_oStack.clear();
		m_oExitCode = exit.m_iCode;
		return Value{};
	} catch (exception::VMError& ex) {
		m_bFailed = true;
		const auto positions = StackTrace();
		bloop::BloopString msg = BLOOPTEXT("\nruntime error\n\n");

		msg += bloop::fmt::format(BLOOPTEXT("\n{}\n\n"), ex.what());

		for (const auto& src : positions)
			msg += FormatStackTraceMessage(src) + '\n';

		std::cout << msg << '\n';
		Push({});
	}

	return m_oStack.size() ? m_oStack.front() : Value{};
}

VM::ExecutionReturnCode VM::RunFrame() {
	ExecutionReturnCode returnCode{};

	auto& chunk = m_refMetaData.m_oVMData.m_oChunks[m_pCurrentFrame->m_pChunk->m_uMetadata];
	m_pCurrentFrame->m_pIpBase = chunk.m_oByteCode.data();
	m_pCurrentFrame->m_pIp = m_pCurrentFrame->m_pIpBase;
	const auto end = m_pCurrentFrame->m_pIp + chunk.m_oByteCode.size();

	while (m_pCurrentFrame->m_pIp != end) {
		returnCode = InterpretOpCode(static_cast<bloop::bytecode::EOpCode>(*m_pCurrentFrame->m_pIp++));

		if (returnCode != ExecutionReturnCode::rc_continue) {
			break;
		}
	}

	return returnCode;
}
void VM::RunGlobal() {
	m_pCurrentFrame = &m_oFrames.emplace_back(&m_oGlobalChunk, 0u);

	for (bloop::BloopSize i{}; const auto& native : bloop::standard::g_Natives) {
		m_oGlobals[i++] = bloop::standard::FromDefinitionToObject(m_oHeap, native, m_oEnvironment);
	}

	const auto returnCode = RunFrame();

	Value ret;
	if (returnCode == ExecutionReturnCode::rc_return_value)
		ret = Pop();

	m_oFrames.clear();
	m_pCurrentFrame = nullptr;

	// returning from inside a loop can leave temporaries behind (a ranged for keeps its iterator on the stack)
	m_oStack.clear();
	Push(ret);
}
void VM::RunFunction(Function* fn) {
	PushFrame(fn);
	const auto returnCode = RunFrame();

	if (returnCode == ExecutionReturnCode::rc_throw)
		return;

	Value ret;
	if (returnCode == ExecutionReturnCode::rc_return_value)
		ret = Pop();

	m_oGC.CloseUpValues(m_oStack.data() + m_oFrames.back().m_uBase);
	PopFrame();
	Push(ret);
}
void VM::RunClosure(Object* closure)
{
	PushFrame(closure);
	const auto returnCode = RunFrame();

	if (returnCode == ExecutionReturnCode::rc_throw)
		return;

	Value ret;
	if (returnCode == ExecutionReturnCode::rc_return_value)
		ret = Pop();

	m_oGC.CloseUpValues(m_oStack.data() + m_oFrames.back().m_uBase);
	PopFrame();
	Push(ret);
}
void VM::Exit(bloop::BloopInt code) {
	throw exception::ExitRequest{ code };
}

Value VM::Call(const Value& callee, std::span<const Value> args) {

	if (!callee.IsCallable())
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not callable"), callee.TypeToString()));

	if (m_oStack.size() + args.size() + 1u >= BLOOP_MAX_STACK)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

	// throws inside the callee must not unwind into the frame of the native that called it
	struct BoundaryGuard {
		VM& vm;
		bloop::BloopSize previous;
		~BoundaryGuard() { vm.m_uThrowBoundary = previous; }
	} guard{ *this, m_uThrowBoundary };
	m_uThrowBoundary = m_oFrames.size();

	const auto argc = args.size();
	const auto base = m_oStack.size();

	// (args may point into the stack, which is fine: it never reallocates)
	for (const auto& arg : args)
		Push(arg);

	auto* obj = callee.obj;

	switch (obj->type) {
	case Object::Type::ot_function:
		if (obj->function->m_uParamCount != argc)
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, obj->function->m_uParamCount));
		RunFunction(obj->function);
		break;
	case Object::Type::ot_closure:
		if (obj->closure.function->m_uParamCount != argc)
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, obj->closure.function->m_uParamCount));
		RunClosure(obj); // the frame keeps the closure alive
		break;
	case Object::Type::ot_nativefunction: {
		const auto* native = obj->nativeFunction;
		if (native->m_uParamCount != bloop::VARIADIC_PARAMETER_COUNT && native->m_uParamCount != argc)
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, native->m_uParamCount));

		const auto result = native->m_pFunction(*this, std::span<const Value>(m_oStack.data() + base, argc));
		m_oStack.resize(base);
		return result;
	}
	default:
		assert(false);
		break;
	}

	// RunFunction / RunClosure leave the return value on the stack
	assert(m_oStack.size() == base + 1u);
	return Pop();
}
