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
