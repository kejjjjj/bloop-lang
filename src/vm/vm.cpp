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

using namespace bloop::vm;

std::vector<Value> VM::BuildConstants(const std::vector<bloop::ConstantData>& constants) {
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
VM::VM(bloop::metadata::Metadata& metadata)
	: m_oHeap(&m_oGC), m_oGC(this), m_refMetaData(metadata) {
	
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
VM::~VM() {
	
	// if the user never called "Run", then nothing needs to be cleared
	if (!m_oStack.empty()) {
		//assert(m_oStack.size() == 1); //something leaked if not true
		m_oStack.clear(); //free everything for the GC
		m_oGlobals.clear(); // let the gc get rid of these
		m_oGlobalChunk.m_oConstants.clear();
		m_oFunctions.clear();
		m_oGC.MajorGC(); //clear everything

	}

	//assert(m_oGC.GetAllocatedSize() == 0u);
}

#include <chrono>
#include <iostream>
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
		#ifdef BLOOP_TEST
		RunGlobal();
		//if (func) {
		//	RunFunction(func);
		//	assert(m_oStack.size() == 1u);
		//}
		//m_oGC.MajorGC();
		#else
		Benchmark("glob+main", [&]() {
			RunGlobal();
			//if (func) {
			//	RunFunction(func);
			//	assert(m_oStack.size() == 1u);
			//}
			//m_oGC.MajorGC();
		});
		#endif

	} catch (exception::VMError& ex) {
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

	for (bloop::BloopUInt i{}; const auto& native : bloop::standard::g_Natives) {
		m_oGlobals[i++] = bloop::standard::FromDefinitionToObject(m_oHeap, native);
	}

	const auto returnCode = RunFrame();

	Value ret;
	if (returnCode == ExecutionReturnCode::rc_return_value)
		ret = Pop();

	m_oFrames.clear();
	m_pCurrentFrame = nullptr;
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
void VM::RunClosure(Closure* closure)
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