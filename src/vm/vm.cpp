#include "vm.hpp"
#include "bytecode/function/bc_function.hpp"
#include "bytecode/defs.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
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
			vals.emplace_back(Value{ m_oHeap.AllocString(const_cast<char*>(data.data()), data.size()) });
		} else {
			vals.emplace_back(Value{ c });
		}
	}
	return vals;
}

[[nodiscard]] static auto ConvertPositions(const std::vector<bloop::bytecode::CInstructionPosition>& v) {
	std::vector<CInstructionPosition> ret;
	ret.reserve(v.size());
	for (auto& var : v)
		ret.push_back(CInstructionPosition{ var.m_uByteOffset, var.m_oPosition });
	return ret;
}
[[nodiscard]] static auto ConvertCaptures(const std::vector<bloop::bytecode::vmdata::Capture>& v) {
	std::vector<Capture> ret;
	ret.reserve(v.size());
	for (auto& var : v)
		ret.push_back(Capture{ .m_uSlot = var.m_uSlot, .m_bIsLocal = var.m_bIsLocal });
	return ret;
}
VM::VM(const bloop::bytecode::VMByteCode& data)
	: m_oHeap(&m_oGC), m_oGC(this) {

	m_oGlobalChunk.m_oConstants = BuildConstants(data.chunk.m_oConstants);
	m_oGlobalChunk.m_oByteCode = data.chunk.m_oByteCode;
	m_oGlobals.resize(data.numGlobals);
	m_oFunctions.reserve(data.functions.size());

	for (const auto& f : data.functions) {
		m_oFunctions.emplace_back(Function{
			.chunk = {
				.m_oConstants = BuildConstants(f.chunk.m_oConstants),
				.m_oByteCode = f.chunk.m_oByteCode,
				.m_oPositions = ConvertPositions(f.chunk.m_oPositions)
			},
			.m_uParamCount = f.m_uParamCount,
			.m_uLocalCount = f.m_uLocalCount,
			.m_oCaptures = ConvertCaptures(f.m_oCaptures)
		});
	}

	for (auto idx = std::size_t{ 0 }; auto& f : m_oFunctions)
		m_oFunctionTable[data.functions[idx++].m_sName ] = &f;

	m_oStack.reserve(BLOOP_MAX_STACK);
	m_oFrames.reserve(BLOOP_MAX_FRAMES);
}
VM::~VM() {
	
	// if the user never called "Run", then nothing needs to be cleared
	if (!m_oStack.empty()) {
		//assert(m_oStack.size() == 1); //something leaked if not true
		m_oStack.clear(); //free everything for the GC
		m_oGlobals.clear(); // let the gc get rid of these
		m_oFunctions.clear();
		m_oGC.Collect(); //clear everything

	}

	assert(m_oGC.GetAllocatedSize() == 0u);
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

Value VM::Run(const bloop::BloopString& entryFuncName) {

	if (!m_oFunctionTable.contains(entryFuncName))
		throw exception::VMError(BLOOPTEXT("couldn't find the entry function: " + entryFuncName));

	const auto func = m_oFunctionTable.at(entryFuncName);

	try {
		#ifdef BLOOP_TEST
			RunGlobal();
			RunFunction(func);
			assert(m_oStack.size() == 1u);
			m_oGC.Collect();
		#else
		Benchmark("glob+main", [&]() {
			RunGlobal();
			RunFunction(func);
			assert(m_oStack.size() == 1u);
			m_oGC.Collect();
		});
		#endif

	} catch (exception::VMError& ex) {
		bloop::BloopString msg;
		if (m_pCurrentFrame) {
			const auto& src = m_pCurrentFrame->GetCurrentPosition();
			msg = bloop::fmt::format("\n\nruntime error:\n\n{}\nat [{}, {}]", ex.what(), std::get<0>(src.pos), std::get<1>(src.pos));
		} else {
			msg = bloop::fmt::format("\n\nruntime error:\n\n{}", ex.what());
		}
		std::cout << msg << '\n';

		while(m_oFrames.size())
			PopFrame();

		Push({});
	}

	//std::cout << bloop::fmt::format("\nreturned: {} : {}\n", m_oStack.front().ValueToString(), m_oStack.front().TypeToString());
	return m_oStack.front();
}
VM::ExecutionReturnCode VM::RunFrame() {
	ExecutionReturnCode returnCode{};

	while (m_pCurrentFrame->m_uIp != m_pCurrentFrame->m_pChunk->m_oByteCode.size()) {
		auto& bytecode = m_pCurrentFrame->m_pChunk->m_oByteCode;
		returnCode = InterpretOpCode(static_cast<bloop::bytecode::EOpCode>(bytecode[m_pCurrentFrame->m_uIp++]));

		if (returnCode != ExecutionReturnCode::rc_continue) {
			break;
		}
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

	if (returnCode == ExecutionReturnCode::rc_throw)
		return;

	m_oGC.CloseUpValues(m_oStack.data());
	const Value ret = returnCode == ExecutionReturnCode::rc_return_value ? Pop() : Value();
	PopFrame();
	Push(ret);
}
void VM::RunClosure(Closure* closure)
{
	PushFrame(closure);
	const auto returnCode = RunFrame();

	if (returnCode == ExecutionReturnCode::rc_throw)
		return;

	m_oGC.CloseUpValues(m_oStack.data());
	const Value ret = returnCode == ExecutionReturnCode::rc_return_value ? Pop() : Value();
	PopFrame();
	Push(ret);
}