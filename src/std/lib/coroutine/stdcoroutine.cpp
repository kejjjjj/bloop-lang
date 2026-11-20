#include "std/lib/coroutine/stdcoroutine.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"
#include "vm/heap/coroutine.hpp"
#include "utils/fmt.hpp"

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// Coroutines are functions that can pause themselves (yield) and be continued later (resume):
//
//   const co = std.coroutine.create((a) => {
//       const b = std.coroutine.yield(a + 1); // pauses here, resume returns a + 1
//       return b * 2;                         // the next resume's value is what yield returned
//   });
//   std.coroutine.resume(co, 1);  // 2
//   std.coroutine.resume(co, 10); // 20, and the coroutine is dead
//
// The language doesn't know what a yielded value means: whatever resumes the coroutine decides (a scheduler
// can treat it as "wait this many seconds", a generator as the next element...)

namespace {

	Object* CoroutineOf(const Value& value, const char* function) {
		if (value.type != Value::Type::t_object || !value.obj->IsCoroutine())
			Fail(bloop::fmt::format(BLOOPTEXT("std.coroutine.{}: argument 1 must be \"coroutine\", but got \"{}\""), function, value.TypeToString()));
		return value.obj;
	}

	// a suspended coroutine that will run `function` (a script function or closure) when it's first resumed
	Value Create(VM& vm, Value function) {
		if (function.type != Value::Type::t_object || !(function.obj->IsFunction() || function.obj->IsClosure()))
			Fail(bloop::fmt::format(BLOOPTEXT("std.coroutine.create: argument 1 must be a script function, but got \"{}\""), function.TypeToString()));

		// (the argument stays on the stack, a root: AllocCoroutine reads it after its allocation)
		return Value{ vm.GetHeap().AllocCoroutine(function) };
	}

	// runs the coroutine until it yields or returns, and returns the value that it yielded or returned.
	// The first resume passes the arguments to the coroutine's function, later ones pass at most one value
	// (what the yield returns inside the coroutine)
	Value Resume(VM& vm, Value coroutine, NativeArgs args) {
		return vm.ResumeCoroutine(CoroutineOf(coroutine, "resume"), args);
	}

	// pauses the running coroutine: its resume returns `value`, and the next resume continues from here
	void Yield(VM& vm, std::optional<Value> value) {
		vm.YieldCoroutine(value.value_or(Value{}));
	}

	// "suspended" (not started, or paused in a yield), "running", "normal" (it resumed another coroutine
	// that hasn't yielded yet) or "dead" (it returned, or an exception ended it)
	bloop::BloopString Status(Value coroutine) {
		return CoroutineState::StatusName(CoroutineOf(coroutine, "status")->coroutine->m_eStatus);
	}

	// the running coroutine, or undefined in the main script
	Value Running(VM& vm) {
		auto* coroutine = vm.RunningCoroutine();
		return coroutine ? Value{ coroutine } : Value{};
	}
}

NativeDef bloop::standard::GetCoroutineDefinitions() {
	return NativeDef(BLOOPTEXT("coroutine"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("create"), Bind<&Create>()),
			NativeField(BLOOPTEXT("resume"), Bind<&Resume>()),
			NativeField(BLOOPTEXT("yield"), Bind<&Yield>()),
			NativeField(BLOOPTEXT("status"), Bind<&Status>()),
			NativeField(BLOOPTEXT("running"), Bind<&Running>()),
		}
	});
}
