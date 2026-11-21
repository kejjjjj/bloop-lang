#pragma once

#include "utils/defs.hpp"
#include "vm/gc/handles.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace bloop::vm {

	struct Object;

	// std.task: coroutines that the host resumes when their wait is over (bloop::Script::RunTasks).
	// Each task is a coroutine that this list keeps alive, and scripts refer to it with a "task" handle (its id)
	struct Task {
		std::uint64_t m_uId{};
		Persistent m_oCoroutine;
		double m_dWaitStart{}; // std.time.now() when it started waiting
		double m_dWakeTime{};  // the first std.time.now() when it can continue
		// cancelled while it was running (or resuming another coroutine): it's removed once it yields back
		bool m_bCancelled{};
	};

	class TaskList {
	public:
		// in the order they were spawned (the order they run in)
		std::vector<Task> m_oTasks;

		[[nodiscard]] std::uint64_t Add(Persistent coroutine, double now) {
			const auto id = m_uNextId++;
			m_oTasks.push_back(Task{ id, std::move(coroutine), now, now, false });
			return id;
		}

		[[nodiscard]] Task* Find(std::uint64_t id) noexcept {
			const auto itr = std::ranges::find_if(m_oTasks, [id](const Task& t) { return t.m_uId == id; });
			return itr == m_oTasks.end() ? nullptr : &*itr;
		}

		[[nodiscard]] Task* FindByCoroutine(Object* coroutine) noexcept {
			const auto itr = std::ranges::find_if(m_oTasks, [coroutine](const Task& t) {
				const auto value = t.m_oCoroutine.Get();
				return value.type == Value::Type::t_object && value.obj == coroutine;
			});
			return itr == m_oTasks.end() ? nullptr : &*itr;
		}

		bool Remove(std::uint64_t id) {
			const auto itr = std::ranges::find_if(m_oTasks, [id](const Task& t) { return t.m_uId == id; });
			if (itr == m_oTasks.end())
				return false;
			m_oTasks.erase(itr);
			return true;
		}

		[[nodiscard]] bool Empty() const noexcept { return m_oTasks.empty(); }

	private:
		std::uint64_t m_uNextId{ 1u };
	};
}
