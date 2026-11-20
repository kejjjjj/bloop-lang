#include "vm/vm.hpp"
#include "vm/frame.hpp"
#include "vm/exception.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/coroutine.hpp"
#include "utils/fmt.hpp"

#include <cassert>
#include <utility>

using namespace bloop::vm;

// A coroutine runs on the VM's own stacks, right above the native call that resumed it:
//
//   [ ...resumer... | resume's arguments | coroutine's frames and values ]
//                                        ^ stackBase / frameBase
//
// Its frames run in a nested Execute that stops when they're all gone (it returned or threw) or when it yields.
// Yielding moves everything above stackBase / frameBase into the coroutine's state, and resuming moves it back
// on top of the stacks, wherever they are by then. Open upvalues that point into the moved values move with them.

using Status = CoroutineState::Status;

namespace {
	CoroutineState& StateOf(Object* coroutine) {
		assert(coroutine && coroutine->IsCoroutine() && coroutine->coroutine);
		return *coroutine->coroutine;
	}
}

Value VM::ResumeCoroutine(Object* coroutine, std::span<const Value> args) {
	auto& state = StateOf(coroutine); // (coroutines never move, and the caller's arguments root this one)

	if (state.m_eStatus == Status::dead)
		ThrowError(BLOOPTEXT("std.coroutine.resume: the coroutine is dead (it has returned, or an exception ended it)"));
	if (state.m_eStatus != Status::suspended)
		ThrowError(BLOOPTEXT("std.coroutine.resume: the coroutine is already running"));

	// every resume nests the dispatch loop in C++
	CheckNativeStack();

	const auto stackBase = m_oStack.size();
	const auto frameBase = m_oFrames.size();

	if (!state.m_bStarted) {
		// like a call: the arguments go to the function's parameters
		const auto& callee = state.m_oCallee;
		auto* obj = callee.obj;
		const auto params = obj->IsClosure() ? obj->closure.function->m_uParamCount : obj->function->m_uParamCount;

		if (args.size() != params)
			ThrowError(bloop::fmt::format(BLOOPTEXT("std.coroutine.resume: passed {} arguments, but the coroutine's function expects {}"), args.size(), params));
		if (stackBase + args.size() + 1u >= BLOOP_MAX_STACK)
			ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

		// (args point into the stack, below stackBase: it never reallocates)
		for (const auto& arg : args)
			Push(arg);

		if (obj->IsClosure())
			PushFrame(obj); // the frame keeps the closure alive
		else
			PushFrame(obj->function);

		state.m_bStarted = true;
		state.m_oCallee = Value{};
	} else {
		if (args.size() > 1u)
			ThrowError(bloop::fmt::format(BLOOPTEXT("std.coroutine.resume: passed {} arguments, but a coroutine that has started takes at most 1 (what its yield returns)"), args.size()));

		const auto sent = args.empty() ? Value{} : args.front(); // (copied before the stack changes)
		RestoreCoroutine(state, stackBase);
		Push(sent); // the result of the std.coroutine.yield call that it's paused in
	}

	// from here on it's running. Whatever happens, the resumer is the running one again afterwards
	struct Running {
		VM& vm;
		CoroutineState& state;
		Object* resumer;
		bloop::BloopSize boundary;
		bloop::BloopBool coroutineBoundary;

		~Running() {
			if (state.m_eStatus == Status::running)
				state.m_eStatus = Status::dead; // a runtime error or std.process.exit ended it (and the script)

			state.m_pResumer = nullptr;
			vm.m_pCoroutine = resumer;
			if (resumer)
				StateOf(resumer).m_eStatus = Status::running;

			vm.m_uThrowBoundary = boundary;
			vm.m_bCoroutineBoundary = coroutineBoundary;
			vm.m_uCallNesting--;
		}
	} running{ *this, state, m_pCoroutine, m_uThrowBoundary, m_bCoroutineBoundary };

	if (m_pCoroutine)
		StateOf(m_pCoroutine).m_eStatus = Status::normal;

	state.m_pResumer = m_pCoroutine;
	state.m_eStatus = Status::running;
	m_pCoroutine = coroutine;

	// an exception that nothing in the coroutine catches stops at its first frame
	m_uThrowBoundary = frameBase;
	m_bCoroutineBoundary = true;

	// yield is only allowed in this loop, not in a callback that a native function runs inside the coroutine
	state.m_uCallNesting = ++m_uCallNesting;

	bool yielded{};
	try {
		yielded = Execute(frameBase);
	} catch (exception::CoroutineUnwind&) {
		// the throw unwound (and closed) all of the coroutine's frames. It ends, and the exception goes on
		// in the resumer once this native returns
		assert(m_oFrames.size() == frameBase && m_oStack.size() == stackBase);
		state.m_eStatus = Status::dead;
		m_eNativeSignal = NativeSignal::raise; // (the value is already in m_oTransfer)
		return Value{};
	}

	if (yielded) {
		SuspendCoroutine(state, stackBase, frameBase);
		state.m_eStatus = Status::suspended;
		return std::exchange(m_oTransfer, Value{});
	}

	// it returned: its frames are gone, and its return value is where its arguments were
	assert(m_oFrames.size() == frameBase && m_oStack.size() == stackBase + 1u);
	state.m_eStatus = Status::dead;
	return Pop();
}

void VM::YieldCoroutine(const Value& value) {
	if (!m_pCoroutine)
		ThrowError(BLOOPTEXT("std.coroutine.yield: not inside a coroutine"));

	// the coroutine's own loop calls natives with m_uCallNesting == its nesting. Anything higher means that
	// a native is running a callback (std.array.transform...), and its C++ frames can't be paused
	if (m_uCallNesting != StateOf(m_pCoroutine).m_uCallNesting)
		ThrowError(BLOOPTEXT("std.coroutine.yield: can't yield inside a function that a native function called (like a std.array.transform callback)"));

	m_oTransfer = value;
	m_eNativeSignal = NativeSignal::yield;
}

void VM::RaiseFromNative(const Value& value) {
	m_oTransfer = value;
	m_eNativeSignal = NativeSignal::raise;
}

void VM::SuspendCoroutine(CoroutineState& state, bloop::BloopSize stackBase, bloop::BloopSize frameBase) {
	assert(state.m_oStack.empty() && state.m_oFrames.empty() && !state.m_pOpenUpValues);
	assert(m_oStack.size() >= stackBase && m_oFrames.size() > frameBase);

	// the values, then the frames (with their stack indices relative to the coroutine's first slot)
	auto* const base = m_oStack.data() + stackBase;
	state.m_oStack.assign(base, m_oStack.data() + m_oStack.size());

	for (auto it = m_oFrames.begin() + static_cast<std::ptrdiff_t>(frameBase); it != m_oFrames.end(); ++it) {
		auto& frame = state.m_oFrames.emplace_back(std::move(*it));
		frame.m_uBase -= stackBase;
		for (auto& handler : frame.m_oExceptionHandlers)
			handler.m_uBase -= stackBase;
	}

	// the open upvalues that point into the moved values are at the front of the list (it's sorted by
	// descending stack address). They now point into the saved stack (which doesn't change size until the
	// coroutine is resumed), and keep the coroutine alive
	UpValue* last{};
	for (auto* uv = m_oGC.m_pOpenUpValues; uv && uv->location >= base; uv = uv->next) {
		uv->location = state.m_oStack.data() + (uv->location - base);
		uv->owner = state.m_pOwner;
		last = uv;
	}
	if (last) {
		state.m_pOpenUpValues = m_oGC.m_pOpenUpValues;
		m_oGC.m_pOpenUpValues = last->next;
		last->next = nullptr;
	}

	m_oFrames.erase(m_oFrames.begin() + static_cast<std::ptrdiff_t>(frameBase), m_oFrames.end());
	m_oStack.resize(stackBase);
	m_pCurrentFrame = m_oFrames.empty() ? nullptr : &m_oFrames.back();

	// the coroutine is old, and it holds the values now (no allocation since they left the stack)
	m_oGC.WriteBarrierAll(state.m_pOwner);
}

void VM::RestoreCoroutine(CoroutineState& state, bloop::BloopSize stackBase) {
	assert(stackBase == m_oStack.size());

	// +1: the result of its yield
	if (stackBase + state.m_oStack.size() + 1u >= BLOOP_MAX_STACK)
		ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));
	if (m_oFrames.size() + state.m_oFrames.size() > BLOOP_MAX_FRAMES)
		ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} call frames"), BLOOP_MAX_FRAMES));

	// (both are reserved to their maximum, so nothing reallocates)
	m_oStack.insert(m_oStack.end(), state.m_oStack.begin(), state.m_oStack.end());
	auto* const base = m_oStack.data() + stackBase;

	for (auto& saved : state.m_oFrames) {
		auto& frame = m_oFrames.emplace_back(std::move(saved));
		frame.m_uBase += stackBase;
		for (auto& handler : frame.m_oExceptionHandlers)
			handler.m_uBase += stackBase;
	}
	m_pCurrentFrame = &m_oFrames.back();

	// its open upvalues point into the stack again. Everything that's already on the stack is below them,
	// so they go to the front of the list
	UpValue* last{};
	for (auto* uv = state.m_pOpenUpValues; uv; uv = uv->next) {
		uv->location = base + (uv->location - state.m_oStack.data());
		uv->owner = nullptr;
		last = uv;
	}
	if (last) {
		last->next = m_oGC.m_pOpenUpValues;
		m_oGC.m_pOpenUpValues = state.m_pOpenUpValues;
		state.m_pOpenUpValues = nullptr;
	}

	state.m_oStack.clear();
	state.m_oFrames.clear();
}
