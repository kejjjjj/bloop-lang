#pragma once

#include "std/native.hpp"

#include <functional>
#include <optional>

namespace bloop::vm {
	class VM;
	struct Value;
}

namespace bloop::standard {
	// std.task
	[[nodiscard]] NativeDef GetTaskDefinitions();

	// continues every task whose wait is over, in the order they were spawned (bloop::Script::RunTasks wraps this).
	// `ran` is called after each one with what VM::ResumeFromHost returned (nothing: a runtime error or
	// std.process.exit ended it, see VM::Failed and VM::GetExitCode). Returning false stops. Returns how many ran
	std::size_t RunDueTasks(bloop::vm::VM& vm, const std::function<bool(const std::optional<bloop::vm::Value>&)>& ran);

	// the std.time.now() when the earliest task can continue (none: no tasks)
	[[nodiscard]] std::optional<double> NextTaskTime(bloop::vm::VM& vm);
}
