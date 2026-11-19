#include "module/module.hpp"
#include "ast/ast.hpp"
#include "vm/vm.hpp"
#include "metadata/metadata.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <utility>
using namespace std::chrono_literals;

// bloop [file [arguments...]]
//   runs the file: the arguments after it are the script's std.process.args,
//   and std.process.exit(code) sets the exit code (1 after an error)
// bloop
//   runs the code in code.def (its imports are relative to the working directory)
//   and prints the bytecode, the run time and the returned value
int main(int argc, char** argv) {

	constexpr auto _code = 
#include "code.def"
	;

	const auto runsFile = argc > 1;

	try {
		bloop::metadata::Metadata metadata;
		metadata.m_bDebugOutput = !runsFile;

		const auto code = runsFile
			? bloop::module::CompileFile(argv[1], metadata)
			: bloop::module::CompileSource(_code, metadata);

		if (code) {
			// the standalone interpreter trusts its scripts
			auto environment = bloop::standard::Environment::Unrestricted();
			for (int i = 2; i < argc; i++)
				environment.m_oArguments.emplace_back(argv[i]);

			bloop::vm::VM vm(metadata, std::move(environment));

			auto ret = vm.Run();

			if (const auto exitCode = vm.GetExitCode())
				return static_cast<int>(*exitCode);
			if (vm.Failed())
				return 1;

			if (!runsFile) {
				std::cout << "\nreturned: " << ret.TypeToString() << " : " << ret.ValueToString() << '\n';
				std::cout << "\n\nfinished!\n";
			}
		}
		
	}
	catch (std::runtime_error& ex) {
		std::cout << ex.what() << '\n';
		return 1;
	}

	return 0;
}
