#pragma once

#include "vm/vm.hpp"
#include "tests/catch_amalgamated.hpp"
#include "utils/hash.hpp"

#include <optional>
#include <ranges>

//saves so much typing
using namespace bloop::vm;


namespace bloop::test {
	struct {} emptyStruct;

	[[nodiscard]] std::optional<Value> TEST_ExecuteFile(const bloop::BloopString& relative_path);
	[[nodiscard]] std::optional<Value> TEST_ExecuteBuffer(bloop::BloopStringView buffer);

	template<typename T>
	struct ExpectedValue {
		Value::Type expectedType;
		T expectedValue;
	};

	template<typename T>
	void AssertValue(const Value& ret, const ExpectedValue<T>& val){

		const auto [ expectedType, expectedValue ] = val;

		REQUIRE(ret.type == expectedType);

		if constexpr (sizeof(T) == 0) {
			return; // undefined
		} else if constexpr (std::is_same_v<T, bloop::BloopBool>) {
			REQUIRE(ret.b == expectedValue);
		} else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
			REQUIRE(static_cast<long long>(ret.i) == static_cast<long long>(expectedValue));
		} else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
			REQUIRE(static_cast<unsigned long long>(ret.u) == static_cast<unsigned long long>(expectedValue));
		} else if constexpr (std::is_floating_point_v<T>) {
			REQUIRE(ret.d == static_cast<double>(expectedValue));
		} else if constexpr (std::is_convertible_v<T, bloop::BloopString>) {
			const auto str = bloop::BloopString(expectedValue);

			REQUIRE(ret.obj->type == Object::Type::ot_string);
			REQUIRE(ret.obj->string.len == static_cast<bloop::BloopInt>(str.size()));

			for(const auto i : std::views::iota(0u, str.size()))
				REQUIRE(str[i] == ret.obj->string.data[i]);
		} else {
			static_assert("can't deduce T");
		}

	}

	template<typename T>
	void CheckConstant(const bloop::BloopString& filename, Value::Type expectedType, T expectedValue) {

		if (const auto ret = bloop::test::TEST_ExecuteFile(filename)) {
			AssertValue(*ret, ExpectedValue<T>{ expectedType, expectedValue });
			return;
		}

		FAIL("code didn't execute correctly");
	}

	template<typename T, bloop::BloopUInt size>
	void CheckArray(const bloop::BloopString& filename, Value::Type expectedType, const std::array<T, size>& vals){
		if (const auto ret = bloop::test::TEST_ExecuteFile(filename)) {
			
			REQUIRE(ret->type == Value::Type::t_object);
			REQUIRE(ret->obj->type == Object::Type::ot_array);
			REQUIRE(ret->obj->array.count == size);

			for(const auto i : std::views::iota(0, ret->obj->array.count)){
				AssertValue(ret->obj->array.values[i], ExpectedValue{ expectedType, vals[i] });
			}

		}

	}

	template<typename T, bloop::BloopUInt size>
	void CheckObject(const bloop::BloopString& filename, Value::Type expectedType, const std::array<std::pair<bloop::BloopString, T>, size>& vals){
		if (const auto ret = bloop::test::TEST_ExecuteFile(filename)) {
			
			REQUIRE(ret->type == Value::Type::t_object);
			REQUIRE(ret->obj->type == Object::Type::ot_object);
			REQUIRE(ret->obj->object.count == size);

			for(const auto i : std::views::iota(0, ret->obj->object.count)){
				auto string = Object(const_cast<bloop::BloopChar*>(vals[i].first.data()), vals[i].first.length());
				string.string.hash = bloop::hash::FNV1a(vals[i].first.data(), vals[i].first.length());

				REQUIRE(ret->obj->object.entries[i].key.IsEqual(&string));
				
				AssertValue(ret->obj->object.entries[i].value, ExpectedValue{ expectedType, vals[i].second });
			}

		}

	}

}


#if defined(_WIN32)
    #define BLOOP_DIRECTORY_SEPARATOR BLOOPTEXT("\\")
    #define BLOOP_DIRECTORY_SEPARATOR_CHAR bloop::BloopChar('\\')
#elif defined(__APPLE__) && defined(__MACH__)
    #define BLOOP_DIRECTORY_SEPARATOR BLOOPTEXT("/")
    #define BLOOP_DIRECTORY_SEPARATOR_CHAR bloop::BloopChar('/')
#else
    #define BLOOP_DIRECTORY_SEPARATOR BLOOPTEXT("/")
    #define BLOOP_DIRECTORY_SEPARATOR_CHAR bloop::BloopChar('/')
#endif

#define MAKE_RELATIVE_PATH(prefix, name) \
bloop::BloopString(BLOOPTEXT(prefix)) + BLOOP_DIRECTORY_SEPARATOR + bloop::BloopString(BLOOPTEXT(name)) + BLOOPTEXT(".bloop")
