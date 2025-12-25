#include "vm.hpp"
#include "bytecode/function/bc_function.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

#include <cassert>

using namespace bloop::vm;

std::vector<Value> BuildConstants(const std::vector<bloop::bytecode::CConstant>& constants) {
	std::vector<Value> vals;
	for (const auto& c : constants)
		vals.emplace_back(Value{ c.m_eDataType, c.m_pConstant });
	return vals;
}

VM::VM(const std::vector<bloop::bytecode::vmdata::Function>& funcs) {

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
	//RunFunction(func);
	Benchmark("main function", [&]() { RunFunction(func); });

}
void VM::RunFunction(Function* fn)
{
	PushFrame(fn);
	auto& bytecode = m_pCurrentFrame->m_pFunction->m_oByteCode;
	while (m_pCurrentFrame->m_uIp != m_pCurrentFrame->m_pFunction->m_oByteCode.size()) {
		if (InterpretOpCode(static_cast<bloop::bytecode::EOpCode>(bytecode[m_pCurrentFrame->m_uIp++])))
			break;
	}

	//PopFrame(); //RETURN exists at the end of each function, so InterpretOpCode handles this already
}