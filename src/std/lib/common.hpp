#pragma once

// helpers shared by the standard library modules

#include "std/bind.hpp"
#include "vm/gc/handles.hpp"
#include "vm/exception.hpp"
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

#include <initializer_list>
#include <optional>
#include <span>

namespace bloop::standard::lib
{
	// misuse of a library function terminates the script
	[[noreturn]] inline void Fail(const bloop::BloopString& message) {
		throw bloop::exception::VMError(message);
	}

	// === (same type and same value, strings compare by content)
	[[nodiscard]] inline bool StrictEquals(bloop::vm::Value a, bloop::vm::Value b) {
		return a.type == b.type && a.IsEqual(b);
	}

	// js style slice bounds: negative values count from the end, everything is clamped to [0, length]
	struct SliceRange {
		bloop::BloopInt begin{};
		bloop::BloopInt end{};
		[[nodiscard]] bloop::BloopInt Count() const noexcept { return end > begin ? end - begin : 0; }
	};
	[[nodiscard]] inline SliceRange ResolveSlice(bloop::BloopInt length, std::optional<bloop::BloopInt> start, std::optional<bloop::BloopInt> end) {
		const auto resolve = [length](bloop::BloopInt i) {
			if (i < 0)
				i += length;
			return i < 0 ? bloop::BloopInt{ 0 } : (i > length ? length : i);
		};
		return { resolve(start.value_or(0)), resolve(end.value_or(length)) };
	}

	// Calls a callback with up to args.size() arguments: like js, a callback that declares fewer
	// parameters only gets the first ones ((x) => ... and (x, i) => ... both work for (element, index)).
	// The result is only valid until the next allocation
	[[nodiscard]] inline bloop::vm::Value CallCallback(bloop::vm::VM& vm, const bloop::vm::Value& callback, std::initializer_list<bloop::vm::Value> args) {
		const auto* fn = callback.obj;
		const auto params =
			fn->IsFunction() ? fn->function->m_uParamCount :
			fn->IsClosure() ? fn->closure.function->m_uParamCount :
			fn->nativeFunction->m_uParamCount;

		auto count = args.size();
		if (params != bloop::VARIADIC_PARAMETER_COUNT && params < count)
			count = params;

		return vm.Call(callback, std::span<const bloop::vm::Value>(args.begin(), count));
	}

	// the maximum number of elements std.array.create and friends allocate
	constexpr bloop::BloopInt MAX_ARRAY_LENGTH = bloop::BloopInt{ 1 } << 26;
}
