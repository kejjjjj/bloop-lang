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

VM::VM(const bloop::bytecode::VMByteCode& data)
	: m_oHeap(this), m_oGC(&m_oHeap) {

	m_oGlobalChunk.m_oConstants = BuildConstants(data.chunk.m_oConstants);
	m_oGlobalChunk.m_oByteCode = data.chunk.m_oByteCode;
	m_oGlobals.resize(data.numGlobals);

	for (const auto& f : data.functions) {
		m_oFunctions.emplace_back(Function{
			.chunk = {.m_oConstants = BuildConstants(f.chunk.m_oConstants), .m_oByteCode = f.chunk.m_oByteCode  },
			.m_uParamCount = f.m_uParamCount,
			.m_uLocalCount = f.m_uLocalCount,
		});

		assert(!m_oFunctionTable.contains(f.m_sName));
		m_oFunctionTable[f.m_sName] = &m_oFunctions.back();
	}
}
VM::~VM() {
	m_oGC.Collect(this); //in case the returned value was a dynamic object

	for (auto& v : m_oGlobals) {
		if (v.type == Value::Type::t_object)
			m_oGC.m_pHeap->FreeObject(v.obj);
	}

	assert(m_oHeap.GetAllocatedSize() == 0u);
}

#include <chrono>
#include <iostream>
#include <functional>
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
	RunGlobal();
	RunFunction(func);
	std::cout << "returned: " << m_oStack.front().ValueToString() << " : " << m_oStack.front().TypeToString() << '\n';
}
VM::ExecutionReturnCode VM::RunFrame()
{
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
	m_pCurrentFrame = nullptr;
}
void VM::RunFunction(Function* fn)
{
	PushFrame(fn);
	const auto returnCode = RunFrame();
	const Value ret = returnCode == ExecutionReturnCode::rc_return_value ? Pop() : Value();
	PopFrame();

	if (m_oFrames.empty()) {
		//program exit
		Push(ret);
		m_oGC.Collect(this);

#if _DEBUG
		if(ret.type == Value::Type::t_object)
			assert(m_oHeap.GetAllocatedSize() == 1u); //let the return value escape to the user
#endif
		return;
	}
	Push(ret);
}