#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{
	struct Object {
		enum class Type { ot_string, ot_array, ot_object, ot_function } type;

		Object(char* _data, std::size_t _len) : type(Type::ot_string), string({.data=_data, .len=_len}) {}

		union {
			struct {
				char* data;
				std::size_t len;
			}string;
		};

		//managed by GC
		bool marked{};
		Object* next{};

		void Free();
		[[nodiscard]] std::size_t GetSize() const;

		[[nodiscard]] bloop::BloopString ValueToString() const;
		[[nodiscard]] bloop::BloopString TypeToString() const;


	};

};