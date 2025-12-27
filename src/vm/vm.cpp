#include "vm.hpp"
#include "bytecode/function/bc_function.hpp"
#include "bytecode/defs.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/heap/dvalue.hpp"

#include <cassert>

using namespace bloop::vm;

std::vector<Value> VM::BuildConstants(const std::vector<bloop::bytecode::CConstant>& constants) {
	std::vector<Value> vals;
	for (const auto& c : constants) {
		if (c.m_eDataType == bloop::EValueType::t_string) {
			vals.emplace_back(Value{ m_oHeap.AllocString(const_cast<char*>(c.m_pConstant.data()), c.m_pConstant.size()) });
		} else {
			vals.emplace_back(Value{ c.m_eDataType, c.m_pConstant });
		}
	}
	return vals;
}

[[nodiscard ]] static std::vector<CInstructionPosition> ConvertPositions(const std::vector<bloop::bytecode::CInstructionPosition>& v) {
	std::vector<CInstructionPosition> ret;
	ret.reserve(v.size());
	for (auto& var : v)
		ret.push_back(CInstructionPosition{ var.m_uByteOffset, var.m_oPosition });
	return ret;
}

VM::VM(const bloop::bytecode::VMByteCode& data)
	: m_oHeap(this), m_oGC(&m_oHeap) {

	m_oGlobalChunk.m_oConstants = BuildConstants(data.chunk.m_oConstants);
	m_oGlobalChunk.m_oByteCode = data.chunk.m_oByteCode;
	m_oGlobals.resize(data.numGlobals);

	for (const auto& f : data.functions) {
		m_oFunctions.emplace_back(Function{
			.chunk = {
				.m_oConstants = BuildConstants(f.chunk.m_oConstants),
				.m_oByteCode = f.chunk.m_oByteCode,
				.m_oPositions = ConvertPositions(f.chunk.m_oPositions)
			},
			.m_uParamCount = f.m_uParamCount,
			.m_uLocalCount = f.m_uLocalCount,
		});

		assert(!m_oFunctionTable.contains(f.m_sName));
		m_oFunctionTable[f.m_sName] = &m_oFunctions.back();
	}
}
VM::~VM() {
	
	// if the user never called "Run", then nothing needs to be cleared
	if (!m_oStack.empty()) {
		m_oStack.clear(); //free everything for the GC
		m_oGlobals.clear(); // let the gc get rid of these
		m_oGC.Collect(this); //clear everything

	}

	assert(m_oHeap.GetAllocatedSize() == 0u);
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

void VM::Run(const bloop::BloopString& entryFuncName) {

	if (!m_oFunctionTable.contains(entryFuncName))
		throw exception::VMError(BLOOPTEXT("couldn't find the entry function: " + entryFuncName));

	const auto func = m_oFunctionTable.at(entryFuncName);

	try {
		RunGlobal();
		RunFunction(func);
	} catch (exception::VMError& ex) {
		bloop::BloopString msg;
		if (auto frame = m_pCurrentFrame) {
			const auto& src = m_pCurrentFrame->GetCurrentPosition();
			msg = bloop::fmt::format("\n\nruntime error:\n\n{}\nat [{}, {}]", ex.what(), std::get<0>(src.pos), std::get<1>(src.pos));
		} else {
			msg = bloop::fmt::format("\n\nruntime error:\n\n{}", ex.what());
		}
		std::cout << msg << '\n';
		return;
	}

	std::cout << bloop::fmt::format("\nreturned: {} : {}\n", m_oStack.front().ValueToString(), m_oStack.front().TypeToString());
}
VM::ExecutionReturnCode VM::RunFrame() {
	auto& bytecode = m_pCurrentFrame->m_pChunk->m_oByteCode;
	ExecutionReturnCode returnCode{};

	while (m_pCurrentFrame->m_uIp != m_pCurrentFrame->m_pChunk->m_oByteCode.size()) {
		returnCode = InterpretOpCode(static_cast<bloop::bytecode::EOpCode>(bytecode[m_pCurrentFrame->m_uIp++]));

		if (returnCode != ExecutionReturnCode::rc_continue)
			break;
	}

	return returnCode;
}
void VM::RunGlobal() {
	m_pCurrentFrame = &m_oFrames.emplace_back(&m_oGlobalChunk, 0u);
	[[maybe_unused]] const auto returnCode = RunFrame();
	m_oFrames.clear();
	m_pCurrentFrame = nullptr;
}
void VM::RunFunction(Function* fn) {
	PushFrame(fn);
	const auto returnCode = RunFrame();
	const Value ret = returnCode == ExecutionReturnCode::rc_return_value ? Pop() : Value();
	PopFrame();

	Push(ret);
	if (m_oFrames.empty()) {
		//program exit
		return m_oGC.Collect(this);
	}
}