#include "bytecode/build.hpp"
#include "ast/function.hpp"
#include "bytecode/function/bc_function.hpp"
#include "bytecode/global/bc_global.hpp"

using namespace bloop::bytecode;

void bloop::bytecode::BuildByteCode(bloop::ast::Program* code, bloop::metadata::Metadata& metadata) {

	CByteCodeGlobals globals(code);
	metadata.m_oVMData.AddChunk(globals.Generate(metadata));

	for (const auto& stmt : code->m_oStatements) {
		if (stmt->IsFunction()) {
			CByteCodeFunction f(dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get()));
			f.Generate(metadata);
		}
	}

	metadata.m_oVMData.m_pGlobalChunk = &metadata.m_oVMData.m_oChunks.front();
}