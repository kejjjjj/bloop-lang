#include "module/module.hpp"
#include "ast/ast.hpp"
#include "vm/vm.hpp"
#include "metadata/metadata.hpp"
#include "vm/tasks.hpp"
#include "std/lib/task/stdtask.hpp"

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

			// std.task: the tasks that the script started keep running until they're all done
			// (or one of them fails, or calls std.process.exit)
			bool failed{};
			while (!vm.GetExitCode() && !vm.Failed() && !failed && !vm.GetTasks().Empty()) {
				if (const auto next = bloop::standard::NextTaskTime(vm); next && *next > vm.Now())
					std::this_thread::sleep_for(std::chrono::duration<double>(*next - vm.Now()));

				bloop::standard::RunDueTasks(vm, [&](const std::optional<bloop::vm::Value>& result) {
					failed = !result;
					return !failed;
				});
			}
			if (failed && !vm.GetExitCode())
				return 1;

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
