#pragma once

// Typed bindings for native functions.
//
// Write a native as a normal C++ function and let Bind generate the argument checks and conversions:
//
//   bloop::BloopDouble Pow(bloop::BloopDouble base, bloop::BloopDouble exponent);
//   NativeField(BLOOPTEXT("pow"), Bind<&Pow>())
//
// Calling std.pow("2", 3) then terminates the script with: argument 1 must be "number", but got "string".
//
// Parameters (in this order):
//   bloop::vm::VM&       optional, first: the calling VM (for allocations, the GC...)
//   BloopBool            boolean only
//   BloopInt             int, or uint if it fits
//   BloopUInt            uint, or int if it isn't negative
//   BloopDouble          any number (int, uint, double)
//   StringArg, ArrayArg, ObjectArg, CallableArg
//                        views of an argument of that type (see below)
//   bloop::vm::Value     anything, unchecked
//   std::variant<...>    any of the alternatives above, the first one that accepts the argument wins
//   NativeArgs           optional, last: the remaining arguments (makes the native variadic)
//
// Return types:
//   void (-> undefined), Value, BloopBool, integers, floating point, Object*, strings (allocated as a bloop string),
//   the argument views, and std::optional of any of these (std::nullopt -> undefined).
//   Return std::nullopt for failures a script can't prevent (bad input, missing files...);
//   throw bloop::exception::VMError for misuse (it terminates the script).
//
// GC safety: the argument views refer to the argument's slot on the VM stack, which the GC keeps up to date.
// They stay valid when the native allocates, but anything read from them (a string_view, an Object*, a Value)
// does not: read it again after an allocation, or root it with RootScope (vm/gc/handles.hpp).

#include "std/native.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

#include <cassert>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace bloop::standard
{
	// base of the argument views: a reference to an argument slot on the VM stack
	class ArgView {
	public:
		explicit ArgView(const bloop::vm::Value& slot) noexcept : m_pSlot(&slot) {}

		// the argument's current value (only valid until the next allocation)
		[[nodiscard]] inline const bloop::vm::Value& Get() const noexcept { return *m_pSlot; }
		[[nodiscard]] inline bloop::vm::Object* Obj() const noexcept { return m_pSlot->obj; }

	private:
		const bloop::vm::Value* m_pSlot;
	};

	struct StringArg : ArgView {
		using ArgView::ArgView;
		// only valid until the next allocation
		[[nodiscard]] inline bloop::BloopStringView View() const noexcept {
			return { Obj()->string.data, static_cast<std::size_t>(Obj()->string.len) };
		}
		[[nodiscard]] inline bloop::BloopString Str() const { return bloop::BloopString(View()); }
		[[nodiscard]] inline bloop::BloopInt Length() const noexcept { return Obj()->string.len; }
	};

	struct ArrayArg : ArgView {
		using ArgView::ArgView;
		[[nodiscard]] inline bloop::BloopInt Count() const noexcept { return Obj()->array.count; }
		// only valid until the next allocation
		[[nodiscard]] inline const bloop::vm::Value& operator[](bloop::BloopInt index) const noexcept {
			assert(index >= 0 && index < Count());
			return Obj()->array.values[index];
		}
	};

	struct ObjectArg : ArgView {
		using ArgView::ArgView;
	};

	struct CallableArg : ArgView {
		using ArgView::ArgView;
	};

	namespace detail
	{
		template<typename>
		inline constexpr bool always_false = false;

		using bloop::vm::Value;
		using VT = bloop::vm::Value::Type;

		// how a parameter type accepts / converts an argument
		template<typename T>
		struct ArgTraits {
			static_assert(always_false<T>, "unsupported native parameter type, see std/bind.hpp for the supported types");
		};

		template<> struct ArgTraits<Value> {
			static bool Accepts(const Value&) noexcept { return true; }
			static Value From(const Value& v) noexcept { return v; }
			static bloop::BloopString Name() { return BLOOPTEXT("any"); }
		};
		template<> struct ArgTraits<bloop::BloopBool> {
			static bool Accepts(const Value& v) noexcept { return v.type == VT::t_bool; }
			static bloop::BloopBool From(const Value& v) noexcept { return v.b; }
			static bloop::BloopString Name() { return BLOOPTEXT("boolean"); }
		};
		template<> struct ArgTraits<bloop::BloopInt> {
			static bool Accepts(const Value& v) noexcept {
				return v.type == VT::t_int
					|| (v.type == VT::t_uint && v.u <= static_cast<bloop::BloopUInt>(std::numeric_limits<bloop::BloopInt>::max()));
			}
			static bloop::BloopInt From(const Value& v) noexcept { return v.type == VT::t_int ? v.i : static_cast<bloop::BloopInt>(v.u); }
			static bloop::BloopString Name() { return BLOOPTEXT("int"); }
		};
		template<> struct ArgTraits<bloop::BloopUInt> {
			static bool Accepts(const Value& v) noexcept { return v.type == VT::t_uint || (v.type == VT::t_int && v.i >= 0); }
			static bloop::BloopUInt From(const Value& v) noexcept { return v.type == VT::t_uint ? v.u : static_cast<bloop::BloopUInt>(v.i); }
			static bloop::BloopString Name() { return BLOOPTEXT("uint"); }
		};
		template<> struct ArgTraits<bloop::BloopDouble> {
			static bool Accepts(const Value& v) noexcept { return v.type == VT::t_double || v.type == VT::t_int || v.type == VT::t_uint; }
			static bloop::BloopDouble From(const Value& v) noexcept {
				switch (v.type) {
				case VT::t_int:  return static_cast<bloop::BloopDouble>(v.i);
				case VT::t_uint: return static_cast<bloop::BloopDouble>(v.u);
				default:         return v.d;
				}
			}
			static bloop::BloopString Name() { return BLOOPTEXT("number"); }
		};
		template<> struct ArgTraits<StringArg> {
			static bool Accepts(const Value& v) noexcept { return v.IsString(); }
			static StringArg From(const Value& v) noexcept { return StringArg(v); }
			static bloop::BloopString Name() { return BLOOPTEXT("string"); }
		};
		template<> struct ArgTraits<ArrayArg> {
			static bool Accepts(const Value& v) noexcept { return v.IsArray(); }
			static ArrayArg From(const Value& v) noexcept { return ArrayArg(v); }
			static bloop::BloopString Name() { return BLOOPTEXT("array"); }
		};
		template<> struct ArgTraits<ObjectArg> {
			static bool Accepts(const Value& v) noexcept { return v.IsObject(); }
			static ObjectArg From(const Value& v) noexcept { return ObjectArg(v); }
			static bloop::BloopString Name() { return BLOOPTEXT("object"); }
		};
		template<> struct ArgTraits<CallableArg> {
			static bool Accepts(const Value& v) { return v.IsCallable(); }
			static CallableArg From(const Value& v) noexcept { return CallableArg(v); }
			static bloop::BloopString Name() { return BLOOPTEXT("function"); }
		};
		template<typename... Ts> struct ArgTraits<std::variant<Ts...>> {
			static bool Accepts(const Value& v) { return (ArgTraits<Ts>::Accepts(v) || ...); }
			static std::variant<Ts...> From(const Value& v) {
				std::optional<std::variant<Ts...>> result;
				// the first alternative that accepts the value
				([&] {
					if (!result && ArgTraits<Ts>::Accepts(v))
						result.emplace(ArgTraits<Ts>::From(v));
				}(), ...);
				assert(result);
				return *result;
			}
			static bloop::BloopString Name() {
				bloop::BloopString name;
				((name += (name.empty() ? BLOOPTEXT("") : BLOOPTEXT("\" or \"")) + ArgTraits<Ts>::Name()), ...);
				return name;
			}
		};

		// splits a native's signature into [VM&] parameters... [NativeArgs]
		template<typename F>
		struct BindTraits;

		template<typename R, typename... A>
		struct BindTraits<R(*)(A...)> {
			using Return = R;
			using All = std::tuple<A...>;
			static constexpr std::size_t total = sizeof...(A);

			template<std::size_t I>
			using At = std::remove_cvref_t<std::tuple_element_t<I, All>>;

			static constexpr bool takesVM = [] {
				if constexpr (total == 0u) return false;
				else return std::is_same_v<std::tuple_element_t<0, All>, bloop::vm::VM&>;
			}();
			static constexpr bool variadic = [] {
				if constexpr (total == 0u) return false;
				else return std::is_same_v<At<total - 1u>, NativeArgs>;
			}();

			static constexpr std::size_t first = takesVM ? 1u : 0u;
			static constexpr std::size_t count = total - first - (variadic ? 1u : 0u); // checked parameters

			template<std::size_t I>
			using Param = At<first + I>;
		};
		template<typename R, typename... A>
		struct BindTraits<R(*)(A...) noexcept> : BindTraits<R(*)(A...)> {};

		template<typename T>
		void CheckArg(const Value& v, std::size_t index) {
			if (!ArgTraits<T>::Accepts(v)) {
				throw bloop::exception::VMError(bloop::fmt::format(BLOOPTEXT("argument {} must be \"{}\", but got \"{}\""),
					index + 1u, ArgTraits<T>::Name(), v.TypeToString()));
			}
		}

		template<typename T>
		struct is_optional : std::false_type {};
		template<typename T>
		struct is_optional<std::optional<T>> : std::true_type {};

		template<typename R>
		[[nodiscard]] Value ToValue([[maybe_unused]] bloop::vm::VM& vm, R&& result) {
			using T = std::remove_cvref_t<R>;

			if constexpr (std::is_same_v<T, Value>) {
				return result;
			} else if constexpr (std::is_same_v<T, bool>) {
				return Value(static_cast<bloop::BloopBool>(result));
			} else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
				return Value(static_cast<bloop::BloopInt>(result));
			} else if constexpr (std::is_integral_v<T>) {
				return Value(static_cast<bloop::BloopUInt>(result));
			} else if constexpr (std::is_floating_point_v<T>) {
				return Value(static_cast<bloop::BloopDouble>(result));
			} else if constexpr (std::is_same_v<T, bloop::vm::Object*>) {
				return Value(result);
			} else if constexpr (std::is_base_of_v<ArgView, T>) {
				return result.Get();
			} else if constexpr (std::is_convertible_v<const T&, bloop::BloopStringView>) {
				const bloop::BloopStringView str = result;
				return Value(vm.GetHeap().AllocString(str.data(), str.size()));
			} else if constexpr (is_optional<T>::value) {
				if (!result)
					return Value{};
				return ToValue(vm, *std::forward<R>(result));
			} else {
				static_assert(always_false<T>, "unsupported native return type, see std/bind.hpp for the supported types");
			}
		}

		template<auto Fn, std::size_t... I>
		[[nodiscard]] Value Invoke([[maybe_unused]] bloop::vm::VM& vm, [[maybe_unused]] NativeArgs args, std::index_sequence<I...>) {
			using Traits = BindTraits<decltype(Fn)>;

			// check everything first (left to right), so that the error always names the first bad argument
			(CheckArg<typename Traits::template Param<I>>(args[I], I), ...);

			const auto call = [&]() -> decltype(auto) {
				if constexpr (Traits::takesVM && Traits::variadic)
					return Fn(vm, ArgTraits<typename Traits::template Param<I>>::From(args[I])..., args.subspan(Traits::count));
				else if constexpr (Traits::takesVM)
					return Fn(vm, ArgTraits<typename Traits::template Param<I>>::From(args[I])...);
				else if constexpr (Traits::variadic)
					return Fn(ArgTraits<typename Traits::template Param<I>>::From(args[I])..., args.subspan(Traits::count));
				else
					return Fn(ArgTraits<typename Traits::template Param<I>>::From(args[I])...);
			};

			if constexpr (std::is_void_v<typename Traits::Return>) {
				call();
				return Value{};
			} else {
				return ToValue(vm, call());
			}
		}

		template<auto Fn>
		[[nodiscard]] Value Thunk(bloop::vm::VM& vm, NativeArgs args) {
			using Traits = BindTraits<decltype(Fn)>;

			if constexpr (Traits::variadic) {
				if (args.size() < Traits::count) {
					throw bloop::exception::VMError(bloop::fmt::format(BLOOPTEXT("passed {} arguments, but expected at least {}"),
						args.size(), Traits::count));
				}
			} else {
				// the VM already checks the argument count of non-variadic natives
				assert(args.size() == Traits::count);
			}

			return Invoke<Fn>(vm, args, std::make_index_sequence<Traits::count>{});
		}
	}

	// generates a NativeFunction (argument count + checking/converting wrapper) for a C++ function
	template<auto Fn>
	[[nodiscard]] constexpr NativeFunction Bind() noexcept {
		using Traits = detail::BindTraits<decltype(Fn)>;
		static_assert(Traits::count < bloop::VARIADIC_PARAMETER_COUNT, "too many parameters");

		return NativeFunction{
			.m_uParamCount = Traits::variadic ? bloop::VARIADIC_PARAMETER_COUNT : static_cast<bloop::BloopIndex>(Traits::count),
			.m_pFunction = &detail::Thunk<Fn>
		};
	}
}
