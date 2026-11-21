#include "std/lib/task/stdtask.hpp"
#include "std/lib/common.hpp"
#include "vm/tasks.hpp"
#include "vm/heap/heap.hpp"
#include "vm/heap/coroutine.hpp"
#include "utils/fmt.hpp"

#include <cmath>
#include <cstdint>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// std.task: functions that run alongside everything else, and can wait without stopping anything.
//
//   std.task.spawn((door) => {
//       doors.lock(door);
//       std.task.wait(10);   // the rest of the game keeps going meanwhile
//       doors.unlock(door);
//   }, door);
//
// A task is a coroutine that the host continues when its wait is over: spawn runs it right away until its first
// wait, and the host calls bloop::Script::RunTasks (a game: every frame) to continue the ones whose time has come.
// The time is std.time.now(), so a game that gives the VM its own clock (Environment::m_fnClock) gets waits that
// pause with the game. The standalone interpreter keeps running the tasks after the main script until they're all done.
//
// Tasks are coroutines underneath, but the language doesn't know about them: this is an ordinary library on top of
// std.coroutine, and a host could write its own scheduler instead.

namespace {
	constexpr const char* TASK_TYPE = "task";

	Value MakeTaskHandle(VM& vm, std::uint64_t id) {
		return Value{ vm.GetHeap().AllocHandle(vm.GetHandleType(BLOOPTEXT("task")), id) };
	}

	// the id of a task handle
	std::uint64_t TaskId(VM& vm, const Value& value, const char* function) {
		if (value.type != Value::Type::t_object || !value.obj->IsHandle() || value.obj->handle.type != vm.GetHandleType(BLOOPTEXT("task")))
			Fail(bloop::fmt::format(BLOOPTEXT("std.task.{}: argument 1 must be \"{}\", but got \"{}\""), function, TASK_TYPE, value.TypeToString()));
		return value.obj->handle.id;
	}

	// after a task stops running (it waited, returned, or an exception or an error ended it): a task that has
	// finished or was cancelled meanwhile leaves the list
	void Settle(VM& vm, std::uint64_t id) {
		auto& tasks = vm.GetTasks();
		auto* task = tasks.Find(id);
		if (!task)
			return;

		const auto coroutine = task->m_oCoroutine.Get();
		const bool dead = coroutine.type != Value::Type::t_object || coroutine.obj->coroutine->m_eStatus == CoroutineState::Status::dead;
		if (dead || task->m_bCancelled)
			tasks.Remove(id);
	}

	// starts `function` (a script function or closure) as a task with the rest of the arguments, and runs it
	// until it waits or finishes. Returns the task. An exception that the task doesn't catch before its first
	// wait goes on here, in the one that spawned it (later ones go to the host)
	Value Spawn(VM& vm, Value function, NativeArgs args) {
		if (function.type != Value::Type::t_object || !(function.obj->IsFunction() || function.obj->IsClosure()))
			Fail(bloop::fmt::format(BLOOPTEXT("std.task.spawn: argument 1 must be a script function, but got \"{}\""), function.TypeToString()));

		const auto params = function.obj->IsClosure() ? function.obj->closure.function->m_uParamCount : function.obj->function->m_uParamCount;
		if (args.size() != params)
			Fail(bloop::fmt::format(BLOOPTEXT("std.task.spawn: passed {} arguments for the function, but it expects {}"), args.size(), params));

		// (coroutines never move, and the list keeps this one alive from here on)
		auto* coroutine = vm.GetHeap().AllocCoroutine(function);
		const auto id = vm.GetTasks().Add(vm.GetGC().MakePersistent(Value{ coroutine }), vm.Now());

		{
			struct SettleOnExit {
				VM& vm;
				std::uint64_t id;
				~SettleOnExit() { Settle(vm, id); }
			} settle{ vm, id };

			// (what it yields or returns doesn't matter: a task's result is what it does)
			[[maybe_unused]] const auto result = vm.ResumeCoroutine(coroutine, args);
		}

		return MakeTaskHandle(vm, id);
	}

	// pauses the running task for `seconds` (none: until the host's next RunTasks, a game's next frame) while
	// everything else goes on. Returns how many seconds it actually waited (std.time.now() after minus before).
	// Only tasks can wait: the main script or an event handler that has to wait spawns a task
	void Wait(VM& vm, std::optional<bloop::BloopDouble> seconds) {
		auto* coroutine = vm.RunningCoroutine();
		auto* task = coroutine ? vm.GetTasks().FindByCoroutine(coroutine) : nullptr;
		if (!task)
			Fail(BLOOPTEXT("std.task.wait: only a task can wait (start one with std.task.spawn)"));

		const auto time = seconds.value_or(0.0);
		if (!std::isfinite(time) || time < 0.0)
			Fail(BLOOPTEXT("std.task.wait: the time must be a number of seconds that isn't negative"));

		const auto now = vm.Now();
		task->m_dWaitStart = now;
		task->m_dWakeTime = now + time;

		// the host resumes it with the time it waited, which becomes this call's return value
		vm.YieldCoroutine(Value{});
	}

	// stops a task: it won't continue from its wait (anything after the wait never runs). Returns whether it was
	// still alive. A task can cancel itself: it ends at its next wait (or when it returns)
	bloop::BloopBool Cancel(VM& vm, Value task) {
		const auto id = TaskId(vm, task, "cancel");
		auto& tasks = vm.GetTasks();
		auto* entry = tasks.Find(id);
		if (!entry || entry->m_bCancelled)
			return false;

		const auto coroutine = entry->m_oCoroutine.Get();
		if (coroutine.obj->coroutine->m_eStatus == CoroutineState::Status::suspended)
			tasks.Remove(id);
		else
			entry->m_bCancelled = true; // it's running (its frames are on the stack): it leaves when it stops
		return true;
	}

	// whether the task hasn't finished (and wasn't cancelled)
	bloop::BloopBool Alive(VM& vm, Value task) {
		const auto* entry = vm.GetTasks().Find(TaskId(vm, task, "alive"));
		return entry && !entry->m_bCancelled;
	}

	// the running task, or undefined outside of one
	Value Current(VM& vm) {
		auto* coroutine = vm.RunningCoroutine();
		const auto* task = coroutine ? vm.GetTasks().FindByCoroutine(coroutine) : nullptr;
		return task ? MakeTaskHandle(vm, task->m_uId) : Value{};
	}
}

NativeDef bloop::standard::GetTaskDefinitions() {
	return NativeDef(BLOOPTEXT("task"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("spawn"), Bind<&Spawn>()),
			NativeField(BLOOPTEXT("wait"), Bind<&Wait>()),
			NativeField(BLOOPTEXT("cancel"), Bind<&Cancel>()),
			NativeField(BLOOPTEXT("alive"), Bind<&Alive>()),
			NativeField(BLOOPTEXT("current"), Bind<&Current>()),
		}
	});
}

std::size_t bloop::standard::RunDueTasks(VM& vm, const std::function<bool(const std::optional<Value>&)>& ran) {
	auto& tasks = vm.GetTasks();
	const auto now = vm.Now();

	// the ones that are due right now: tasks that this spawns, or that wait again, run on the next call
	std::vector<std::uint64_t> due;
	for (const auto& task : tasks.m_oTasks) {
		if (task.m_dWakeTime <= now && !task.m_bCancelled)
			due.push_back(task.m_uId);
	}

	std::size_t count{};
	for (const auto id : due) {
		auto* task = tasks.Find(id);
		if (!task || task->m_bCancelled)
			continue; // cancelled by one that ran before it

		const auto coroutine = task->m_oCoroutine.Get();
		if (coroutine.obj->coroutine->m_eStatus != CoroutineState::Status::suspended) {
			tasks.Remove(id); // (something else resumed it to its end)
			continue;
		}

		// a yield that isn't a std.task.wait continues on the next call
		const Value waited{ static_cast<bloop::BloopDouble>(now - task->m_dWaitStart) };
		task->m_dWaitStart = now;
		task->m_dWakeTime = now;

		const auto result = vm.ResumeFromHost(coroutine.obj, std::span<const Value>(&waited, 1u));
		count++;
		Settle(vm, id); // finished, failed or cancelled while it ran: it leaves the list

		if (!ran(result))
			break;
	}
	return count;
}

std::optional<double> bloop::standard::NextTaskTime(VM& vm) {
	std::optional<double> earliest;
	for (const auto& task : vm.GetTasks().m_oTasks) {
		if (!earliest || task.m_dWakeTime < *earliest)
			earliest = task.m_dWakeTime;
	}
	return earliest;
}
