#include "vm.hpp"
#include "bytecode/function/bc_function.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

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

VM::VM(const std::vector<bloop::bytecode::vmdata::Function>& funcs) 
	: m_oHeap(this), m_oGC(&m_oHeap) {
	for (const auto& f : funcs) {
		m_oFunctions.emplace_back(Function{
			.m_uParamCount = f.m_uParamCount,
			.m_uLocalCount = f.m_uLocalCount,
			.m_oConstants = BuildConstants(f.m_oConstants),
			.m_oByteCode = f.m_oByteCode
		});

		assert(!m_oFunctionTable.contains(f.m_sName));
		m_oFunctionTable[f.m_sName] = &m_oFunctions.back();
	}
}
VM::~VM() {
	m_oGC.Collect(this); //in case the returned value was a dynamic object
	assert(m_oHeap.GetAllocatedSize() == 0u);
}

#include <chrono>
#include <iostream>
template<typename F>
void Benchmark(const char* name, F&& fn) {
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
	RunFunction(func);
	std::cout << "returned: " << m_oStack.front().ValueToString() << " : " << m_oStack.front().TypeToString() << '\n';
}
void VM::RunFunction(Function* fn)
{
	PushFrame(fn);
	auto& bytecode = m_pCurrentFrame->m_pFunction->m_oByteCode;

	ExecutionReturnCode returnCode{};
	while (m_pCurrentFrame->m_uIp != m_pCurrentFrame->m_pFunction->m_oByteCode.size()) {
		returnCode = InterpretOpCode(static_cast<bloop::bytecode::EOpCode>(bytecode[m_pCurrentFrame->m_uIp++]));
		
		if(returnCode != ExecutionReturnCode::rc_continue)
			break;
	}

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