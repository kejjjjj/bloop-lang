#include "bytecode/defs.hpp"
#include "bytecode/function/bc_function.hpp"
#include "std/vm/to_vm.hpp"
#include "utils/fmt.hpp"
#include "vm.hpp"
#include "vm/exception.hpp"
#include "vm/frame.hpp"
#include "vm/heap/dvalue.hpp"

#include <cassert>
#include <chrono>
#include <random>
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

	m_oRandom.Seed(m_oEnvironment.m_oRandomSeed ? *m_oEnvironment.m_oRandomSeed
		: (static_cast<std::uint64_t>(std::random_device{}()) << 32u) ^ std::random_device{}()
		  ^ static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
	
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

	m_uInstructionsUsed = 0u;

	m_uStackBase = NativeStackAddress();

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
		ReportError(ex);
		Push({});
	}

	return m_oStack.size() ? m_oStack.front() : Value{};
}
void VM::ReportError(const std::exception& error) {
	m_bFailed = true;
	const auto positions = StackTrace();
	m_oStack.clear();

	bloop::BloopString msg = BLOOPTEXT("\nruntime error\n\n");
	msg += bloop::fmt::format(BLOOPTEXT("\n{}\n\n"), error.what());

	// deep recursion: the first and the last calls are enough
	constexpr bloop::BloopSize shown = 16u;
	for (bloop::BloopSize i = 0; i < positions.size(); i++) {
		if (positions.size() > shown * 2u && i == shown) {
			msg += bloop::fmt::format(BLOOPTEXT("... ({} more calls)\n\n"), positions.size() - shown * 2u);
			i = positions.size() - shown;
		}
		msg += FormatStackTraceMessage(positions[i]) + '\n';
	}

	m_sError = msg;

	if (m_bPrintErrors)
		std::cout << msg << '\n';
}
void VM::SetLimits(const Limits& limits) {
	m_oLimits = limits;
	m_oGC.SetHeapLimit(limits.m_uHeapBytes);
}
std::optional<Value> VM::CallFromHost(const Value& callee, std::span<const Value> args) {
	assert(!IsRunning());

	m_uInstructionsUsed = 0u;
	m_bFailed = false;
	m_sError.clear();
	m_eNativeSignal = NativeSignal::none;
	m_oTransfer = Value{};

	m_uStackBase = NativeStackAddress();

	try {
		auto result = Call(callee, args);
		m_oStack.clear();
		return result;
	} catch (exception::ExitRequest& exit) {
		[[maybe_unused]] const auto positions = StackTrace();
		m_oStack.clear();
		m_oExitCode = exit.m_iCode;
	} catch (exception::VMError& ex) {
		ReportError(ex);
	}
	return std::nullopt;
}

bool VM::Execute(bloop::BloopSize stopDepth) {
	assert(m_oFrames.size() > stopDepth);

	for (;;) {
		auto* frame = m_pCurrentFrame;

		// falling off the end of a chunk returns undefined
		if (frame->m_pIp == frame->m_pIpEnd) {
			Push(Value{});
			if (ReturnFromFrame(stopDepth))
				return false;
			continue;
		}

		// a script that the host doesn't trust can't run forever
		if (m_oLimits.m_uInstructions && ++m_uInstructionsUsed > m_oLimits.m_uInstructions)
			ThrowError(bloop::fmt::format(BLOOPTEXT("the script ran too long (more than {} instructions)"), m_oLimits.m_uInstructions));

		const auto op = static_cast<bloop::bytecode::EOpCode>(*frame->m_pIp++);

		// CALL pushes a frame (or runs a native) and the loop carries on in the callee
		const auto returnCode = op == bloop::bytecode::EOpCode::CALL ? CallOp() : InterpretOpCode(op);

		if (returnCode == ExecutionReturnCode::rc_continue)
			continue;

		// (only the loop of the coroutine itself gets here: yield checks that)
		if (returnCode == ExecutionReturnCode::rc_yield)
			return true;

		if (returnCode == ExecutionReturnCode::rc_return)
			Push(Value{});

		if (ReturnFromFrame(stopDepth))
			return false;
	}
}
bool VM::ReturnFromFrame(bloop::BloopSize stopDepth) {
	// the return value is on top of the stack, where it stays a root until the frame is gone
	const auto base = m_oFrames.back().m_uBase;
	m_oGC.CloseUpValues(m_oStack.data() + base); // (doesn't allocate)

	const Value ret = m_oStack.back();
	PopFrame(); // the stack goes back to where the caller put the arguments
	Push(ret);

	return m_oFrames.size() == stopDepth;
}
void VM::RunGlobal() {
	StartFrame(m_oFrames.emplace_back(&m_oGlobalChunk, 0u));

	assert(!m_refMetaData.m_oNatives.empty()); // filled by the resolver
	for (bloop::BloopSize i{}; const auto& native : m_refMetaData.m_oNatives) {
		m_oGlobals[i++] = bloop::standard::FromDefinitionToObject(m_oHeap, native, m_oEnvironment);
	}

	// returning from inside a loop can leave temporaries behind (a ranged for keeps its iterator on the stack),
	// but returning pops everything above the frame's base, which is 0: the stack ends up with just the value
	[[maybe_unused]] const auto yielded = Execute(0u);
	assert(!yielded);
	assert(m_oStack.size() == 1u && m_oFrames.empty() && !m_pCurrentFrame);
}
void VM::CheckArgumentCount(bloop::BloopSize argc, bloop::BloopSize paramCount) {
	// strict: a script function has to get exactly as many arguments as it has parameters
	if (argc != paramCount)
		ThrowError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, paramCount));
}

void VM::Exit(bloop::BloopInt code) {
	throw exception::ExitRequest{ code };
}

Value VM::Call(const Value& callee, std::span<const Value> args) {

	if (!callee.IsCallable())
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("a value of type \"{}\" is not callable"), callee.TypeToString()));

	// this is the one place where calls recurse in C++ (a native calling a script function calling a native...)
	CheckNativeStack();

	if (m_oStack.size() + args.size() + 1u >= BLOOP_MAX_STACK)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

	// throws inside the callee must not unwind into the frame of the native that called it
	// (and a coroutine can't yield from in here: the native's C++ frames are in between)
	struct BoundaryGuard {
		VM& vm;
		bloop::BloopSize previous;
		bloop::BloopBool previousCoroutine;
		~BoundaryGuard() {
			vm.m_uThrowBoundary = previous;
			vm.m_bCoroutineBoundary = previousCoroutine;
			vm.m_uCallNesting--;
		}
	} guard{ *this, m_uThrowBoundary, m_bCoroutineBoundary };
	m_uThrowBoundary = m_oFrames.size();
	m_bCoroutineBoundary = false;
	m_uCallNesting++;

	const auto argc = args.size();
	const auto base = m_oStack.size();

	// (args may point into the stack, which is fine: it never reallocates)
	for (const auto& arg : args)
		Push(arg);

	auto* obj = callee.obj;
	const auto depth = m_oFrames.size();
	bool yielded{}; // (never: a yield in here is an error, see VM::YieldCoroutine)

	switch (obj->type) {
	case Object::Type::ot_function:
		CheckArgumentCount(argc, obj->function->m_uParamCount);
		PushFrame(obj->function);
		yielded = Execute(depth); // returns when the callee does
		break;
	case Object::Type::ot_closure:
		CheckArgumentCount(argc, obj->closure.function->m_uParamCount);
		PushFrame(obj); // the frame keeps the closure alive
		yielded = Execute(depth);
		break;
	case Object::Type::ot_nativefunction: {
		const auto* native = obj->nativeFunction;
		if (native->m_uParamCount != bloop::VARIADIC_PARAMETER_COUNT && native->m_uParamCount != argc)
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected {}"), argc, native->m_uParamCount));

		const auto result = native->m_pFunction(*this, std::span<const Value>(m_oStack.data() + base, argc));
		m_oStack.resize(base);

		// it asked to throw: nothing above it to catch it in here, so this ends up as an uncaught exception
		if (HandleNativeSignal())
			return Value{};
		return result;
	}
	default:
		assert(false);
		break;
	}

	assert(!yielded);
	(void)yielded;

	// the callee's return leaves its value where the arguments were
	assert(m_oStack.size() == base + 1u);
	return Pop();
}
