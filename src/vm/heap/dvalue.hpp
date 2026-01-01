#pragma once

#include "utils/defs.hpp"
#include <unordered_set>

namespace bloop::vm
{
	struct Function;
	struct Value;
	struct UpValue;

	struct Closure {
		Function* function;
		UpValue** upvalues;
		bloop::BloopUInt numValues;
	};

	struct Object {
		enum class Type { ot_string, ot_array, ot_object, ot_function, ot_closure, ot_upvalue } type;

		Object(char* _data, bloop::BloopInt _len) : type(Type::ot_string), string({.data=_data, .len=_len}) {}
		Object(Function* chunk) : type(Type::ot_function), function(chunk){}
		Object(Function* function, UpValue** upVals, bloop::BloopUInt numVals);
		Object(UpValue* upval) : type(Type::ot_upvalue), upvalue(upval){}

		Object(bloop::BloopInt ucount);

		union {
			struct {
				char* data;
				bloop::BloopInt len;
			}string;
			Function* function;
			struct {
				Value* values;
				bloop::BloopInt count;
			}array;
			Closure closure;
			UpValue* upvalue;
		};

		//managed by GC
		bool marked{};
		Object* next{};

		void Free();
		[[nodiscard]] std::size_t GetSize() const;

		[[nodiscard]] bool IsIndexable() const;
		[[nodiscard]] Value& Index(bloop::BloopInt idx) const;

		[[nodiscard]] bloop::BloopString ValueToString() const;
		[[nodiscard]] bloop::BloopString TypeToString() const;

	private:
		[[nodiscard]] bloop::BloopString ValueToStringInternal(std::unordered_set<const Object*>& seen) const;

	};

};