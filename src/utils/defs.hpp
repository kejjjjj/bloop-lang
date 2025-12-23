#pragma once
#include <string>
#include <tuple>

#ifndef BLOOP_NONCOPYABLE
#define BLOOP_NONCOPYABLE(className) \
        className(const className&) = delete; \
        className& operator=(const className&) = delete
#endif

#ifndef BLOOPTEXT
#define BLOOPTEXT(str) str
#endif

namespace bloop {
	using BloopString = std::string;
	using BloopStringView = std::string_view;
	using BloopChar = char;
	using BloopBool = bool;


#if defined(_WIN32)
#if defined(_WIN64)
	// 64-bit Windows
	using BloopInt = long long;
	using BloopUInt = unsigned long long;
#else
	// 32-bit Windows
	using BloopInt = int;
	using BloopUInt = unsigned int;
#endif
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if defined(__x86_64__) || defined(__aarch64__)
	// 64-bit macOS
	using BloopInt = long long;
	using BloopUInt = unsigned long long;
#else
	// 32-bit macOS
	using BloopInt = int;
	using BloopUInt = unsigned int;
#endif
#else
#if defined(__x86_64__) || defined(__ppc64__)
	// 64-bit non-Windows, non-macOS
	using BloopInt = long long;
	using BloopUInt = unsigned long long;
#else
	// 32-bit non-Windows, non-macOS
	using BloopInt = int;
	using BloopUInt = unsigned int;
#endif
#endif

	using BloopDouble = double;


	enum class EStatus : signed char
	{
		failure,
		success
	};

	enum class EValueType : unsigned char {
		t_undefined,
		t_boolean,
		t_uint,
		t_int,
		t_double,
		t_string,
		t_callable,
		t_array,
		t_object,
	};

	using CodePosition = std::tuple<std::size_t, std::size_t>;
}
