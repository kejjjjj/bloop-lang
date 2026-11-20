#include "std/lib/time/stdtime.hpp"
#include "std/lib/common.hpp"

#include <chrono>
#include <cmath>
#include <thread>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// std.time: clocks, in seconds (doubles)

namespace {

	// a clock for measuring time: it never goes backwards, and it starts at 0 when the VM is created.
	// The host can replace it (Environment::m_fnClock), a game with its game time
	bloop::BloopDouble Now(VM& vm) {
		if (const auto& clock = vm.GetEnvironment().m_fnClock)
			return clock();

		const auto elapsed = std::chrono::steady_clock::now() - vm.GetCreationTime();
		return std::chrono::duration<bloop::BloopDouble>(elapsed).count();
	}

	// the wall clock: seconds since 1970-01-01 00:00 UTC (for timestamps, it can jump when the system clock changes)
	bloop::BloopDouble Unix() {
		const auto sinceEpoch = std::chrono::system_clock::now().time_since_epoch();
		return std::chrono::duration<bloop::BloopDouble>(sinceEpoch).count();
	}

	// blocks the thread for `seconds` (only with Environment::m_bSleep: in a game it would freeze the frame)
	void Sleep(bloop::BloopDouble seconds) {
		if (!std::isfinite(seconds) || seconds < 0.0)
			Fail(BLOOPTEXT("std.time.sleep: the time must be a number of seconds that isn't negative"));

		std::this_thread::sleep_for(std::chrono::duration<bloop::BloopDouble>(seconds));
	}
}

NativeDef bloop::standard::GetTimeDefinitions() {
	return NativeDef(BLOOPTEXT("time"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("now"), Bind<&Now>()),
			NativeField(BLOOPTEXT("unix"), Bind<&Unix>()),
			NativeField(BLOOPTEXT("sleep"), Bind<&Sleep>(), Permission::sleep),
		}
	});
}
