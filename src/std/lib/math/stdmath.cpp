#include "std/lib/math/stdmath.hpp"
#include "std/lib/common.hpp"
#include "utils/fmt.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// std.math
//
//   abs, min, max, clamp, sign  keep the type of their argument (an int stays an int)
//   floor, ceil, round, trunc   return an int (so the result can index an array)
//   everything else             returns a double, with the usual IEEE results: sqrt(-1) is nan, 1 / 0 is inf...
//   random, random_int, seed    a generator per VM, seeded with Environment::m_oRandomSeed

namespace {
	using VT = Value::Type;

	[[nodiscard]] bool IsNumber(const Value& v) noexcept {
		return v.type == VT::t_int || v.type == VT::t_uint || v.type == VT::t_double;
	}
	[[nodiscard]] bool IsNan(const Value& v) noexcept {
		return v.type == VT::t_double && std::isnan(v.d);
	}
	[[nodiscard]] double AsDouble(const Value& v) noexcept {
		switch (v.type) {
		case VT::t_int:  return static_cast<double>(v.i);
		case VT::t_uint: return static_cast<double>(v.u);
		default:         return v.d;
		}
	}
	const Value& CheckNumber(const Value& v, const char* function, std::size_t index) {
		if (!IsNumber(v))
			Fail(bloop::fmt::format(BLOOPTEXT("std.math.{}: argument {} must be \"number\", but got \"{}\""), function, index + 1u, v.TypeToString()));
		return v;
	}

	// a < b for any two numbers, exact for ints (also an int against a uint)
	[[nodiscard]] bool Less(const Value& a, const Value& b) noexcept {
		if (a.type == VT::t_double || b.type == VT::t_double)
			return AsDouble(a) < AsDouble(b);
		if (a.type == VT::t_int && b.type == VT::t_int)
			return a.i < b.i;
		if (a.type == VT::t_uint && b.type == VT::t_uint)
			return a.u < b.u;
		if (a.type == VT::t_int) // int < uint
			return a.i < 0 || static_cast<bloop::BloopUInt>(a.i) < b.u;
		return b.i >= 0 && a.u < static_cast<bloop::BloopUInt>(b.i); // uint < int
	}

	// ---- keeps the type ----

	Value Abs(Value x) {
		CheckNumber(x, "abs", 0u);
		switch (x.type) {
		case VT::t_int:
			if (x.i == std::numeric_limits<bloop::BloopInt>::min())
				Fail(BLOOPTEXT("std.math.abs: the result doesn't fit in an int"));
			return Value{ x.i < 0 ? -x.i : x.i };
		case VT::t_uint:
			return x;
		default:
			return Value{ static_cast<bloop::BloopDouble>(std::fabs(x.d)) };
		}
	}

	// the smallest (or the largest) of one or more numbers, as it was passed. nan if any of them is nan
	template<bool Smallest>
	Value Pick(Value first, NativeArgs rest) {
		const auto* name = Smallest ? "min" : "max";
		Value best = CheckNumber(first, name, 0u);

		for (std::size_t i = 0; i < rest.size(); i++) {
			const auto& v = CheckNumber(rest[i], name, i + 1u);
			if (IsNan(best))
				continue;
			if (IsNan(v) || (Smallest ? Less(v, best) : Less(best, v)))
				best = v;
		}
		return best;
	}
	Value Min(Value first, NativeArgs rest) { return Pick<true>(first, rest); }
	Value Max(Value first, NativeArgs rest) { return Pick<false>(first, rest); }

	// x, but at least low and at most high
	Value Clamp(Value x, Value low, Value high) {
		CheckNumber(x, "clamp", 0u);
		CheckNumber(low, "clamp", 1u);
		CheckNumber(high, "clamp", 2u);

		if (IsNan(low) || IsNan(high) || Less(high, low))
			Fail(BLOOPTEXT("std.math.clamp: the low limit must not be higher than the high limit"));

		if (IsNan(x))
			return x;
		if (Less(x, low))
			return low;
		if (Less(high, x))
			return high;
		return x;
	}

	// -1, 0 or 1 (an int for ints, a double for doubles: nan stays nan)
	Value Sign(Value x) {
		CheckNumber(x, "sign", 0u);
		switch (x.type) {
		case VT::t_int:
			return Value{ static_cast<bloop::BloopInt>((x.i > 0) - (x.i < 0)) };
		case VT::t_uint:
			return Value{ static_cast<bloop::BloopUInt>(x.u > 0u) };
		default:
			if (std::isnan(x.d))
				return x;
			return Value{ static_cast<bloop::BloopDouble>((x.d > 0.0) - (x.d < 0.0)) };
		}
	}

	// ---- rounding: returns an int ----

	template<double(*Round)(double)>
	Value ToInt(Value x, const char* name) {
		CheckNumber(x, name, 0u);
		if (x.type != VT::t_double)
			return x; // already whole

		const auto r = Round(x.d);
		// [-2^63, 2^63): the doubles that fit in an int
		if (!(r >= -9223372036854775808.0 && r < 9223372036854775808.0))
			Fail(bloop::fmt::format(BLOOPTEXT("std.math.{}: the result doesn't fit in an int (the number was {})"), name, x.ValueToString()));
		return Value{ static_cast<bloop::BloopInt>(r) };
	}
	double FloorD(double x) { return std::floor(x); }
	double CeilD(double x) { return std::ceil(x); }
	double RoundD(double x) { return std::round(x); } // halves away from zero
	double TruncD(double x) { return std::trunc(x); }

	Value Floor(Value x) { return ToInt<&FloorD>(x, "floor"); }
	Value Ceil(Value x) { return ToInt<&CeilD>(x, "ceil"); }
	Value Round(Value x) { return ToInt<&RoundD>(x, "round"); }
	Value Trunc(Value x) { return ToInt<&TruncD>(x, "trunc"); }

	// ---- doubles ----

	using D = bloop::BloopDouble;

	D Sqrt(D x) { return std::sqrt(x); }
	D Cbrt(D x) { return std::cbrt(x); }
	D Pow(D base, D exponent) { return std::pow(base, exponent); }
	D Exp(D x) { return std::exp(x); }
	D Log(D x) { return std::log(x); }
	D Log2(D x) { return std::log2(x); }
	D Log10(D x) { return std::log10(x); }
	D Sin(D x) { return std::sin(x); }
	D Cos(D x) { return std::cos(x); }
	D Tan(D x) { return std::tan(x); }
	D Asin(D x) { return std::asin(x); }
	D Acos(D x) { return std::acos(x); }
	D Atan(D x) { return std::atan(x); }
	D Atan2(D y, D x) { return std::atan2(y, x); }
	D Hypot(D x, D y) { return std::hypot(x, y); }
	// a + (b - a) * t: a at t = 0, b at t = 1 (t isn't clamped)
	D Lerp(D a, D b, D t) { return a + (b - a) * t; }
	D ToRadians(D degrees) { return degrees * (std::numbers::pi / 180.0); }
	D ToDegrees(D radians) { return radians * (180.0 / std::numbers::pi); }

	bloop::BloopBool IsNanFn(Value x) { return IsNan(CheckNumber(x, "is_nan", 0u)); }
	bloop::BloopBool IsFinite(Value x) {
		CheckNumber(x, "is_finite", 0u);
		return x.type != VT::t_double || std::isfinite(x.d);
	}

	// ---- random numbers ----

	// a double in [0, 1)
	D RandomFn(VM& vm) { return vm.GetRandom().NextDouble(); }

	// an int in [low, high], both included
	bloop::BloopInt RandomInt(VM& vm, bloop::BloopInt low, bloop::BloopInt high) {
		if (low > high)
			Fail(BLOOPTEXT("std.math.random_int: the low limit must not be higher than the high limit"));

		// (the whole int range wraps around to 0: then any 64 bits will do)
		const auto range = static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low) + 1u;
		auto& random = vm.GetRandom();
		const auto offset = range ? random.Below(range) : random.Next();
		return static_cast<bloop::BloopInt>(static_cast<std::uint64_t>(low) + offset);
	}

	// starts the numbers over: the same seed gives the same numbers
	void Seed(VM& vm, std::variant<bloop::BloopInt, bloop::BloopUInt> seed) {
		const auto value = std::holds_alternative<bloop::BloopInt>(seed)
			? static_cast<std::uint64_t>(std::get<bloop::BloopInt>(seed))
			: static_cast<std::uint64_t>(std::get<bloop::BloopUInt>(seed));
		vm.GetRandom().Seed(value);
	}
}

NativeDef bloop::standard::GetMathDefinitions() {
	return NativeDef(BLOOPTEXT("math"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("pi"), MakeConstant(static_cast<D>(std::numbers::pi))),
			NativeField(BLOOPTEXT("tau"), MakeConstant(static_cast<D>(2.0 * std::numbers::pi))),
			NativeField(BLOOPTEXT("e"), MakeConstant(static_cast<D>(std::numbers::e))),
			NativeField(BLOOPTEXT("inf"), MakeConstant(std::numeric_limits<D>::infinity())),

			NativeField(BLOOPTEXT("abs"), Bind<&Abs>()),
			NativeField(BLOOPTEXT("min"), Bind<&Min>()),
			NativeField(BLOOPTEXT("max"), Bind<&Max>()),
			NativeField(BLOOPTEXT("clamp"), Bind<&Clamp>()),
			NativeField(BLOOPTEXT("sign"), Bind<&Sign>()),

			NativeField(BLOOPTEXT("floor"), Bind<&Floor>()),
			NativeField(BLOOPTEXT("ceil"), Bind<&Ceil>()),
			NativeField(BLOOPTEXT("round"), Bind<&Round>()),
			NativeField(BLOOPTEXT("trunc"), Bind<&Trunc>()),

			NativeField(BLOOPTEXT("sqrt"), Bind<&Sqrt>()),
			NativeField(BLOOPTEXT("cbrt"), Bind<&Cbrt>()),
			NativeField(BLOOPTEXT("pow"), Bind<&Pow>()),
			NativeField(BLOOPTEXT("exp"), Bind<&Exp>()),
			NativeField(BLOOPTEXT("log"), Bind<&Log>()),
			NativeField(BLOOPTEXT("log2"), Bind<&Log2>()),
			NativeField(BLOOPTEXT("log10"), Bind<&Log10>()),
			NativeField(BLOOPTEXT("sin"), Bind<&Sin>()),
			NativeField(BLOOPTEXT("cos"), Bind<&Cos>()),
			NativeField(BLOOPTEXT("tan"), Bind<&Tan>()),
			NativeField(BLOOPTEXT("asin"), Bind<&Asin>()),
			NativeField(BLOOPTEXT("acos"), Bind<&Acos>()),
			NativeField(BLOOPTEXT("atan"), Bind<&Atan>()),
			NativeField(BLOOPTEXT("atan2"), Bind<&Atan2>()),
			NativeField(BLOOPTEXT("hypot"), Bind<&Hypot>()),
			NativeField(BLOOPTEXT("lerp"), Bind<&Lerp>()),
			NativeField(BLOOPTEXT("to_radians"), Bind<&ToRadians>()),
			NativeField(BLOOPTEXT("to_degrees"), Bind<&ToDegrees>()),
			NativeField(BLOOPTEXT("is_nan"), Bind<&IsNanFn>()),
			NativeField(BLOOPTEXT("is_finite"), Bind<&IsFinite>()),

			NativeField(BLOOPTEXT("random"), Bind<&RandomFn>()),
			NativeField(BLOOPTEXT("random_int"), Bind<&RandomInt>()),
			NativeField(BLOOPTEXT("seed"), Bind<&Seed>()),
		}
	});
}
