#pragma once

#include "utils/defs.hpp"
#include <unordered_set>

namespace bloop::vm
{
	struct Function;
	struct Value;
	struct UpValue;
	struct ObjectEntry;

	struct Closure {
		Function* function;
		UpValue** upvalues;
		bloop::BloopUInt numValues;
	};

	struct Object {
		enum class Type { ot_string, ot_array, ot_object, ot_function, ot_closure, ot_upvalue } type;

		Object(bloop::BloopChar* _data, bloop::BloopInt _len) : type(Type::ot_string), string({.data=_data, .len=_len}) {}
		Object(Function* chunk) : type(Type::ot_function), function(chunk){}
		Object(Function* function, UpValue** upVals, bloop::BloopUInt numVals);
		Object(UpValue* upval) : type(Type::ot_upvalue), upvalue(upval){}

		Object(Value* values, bloop::BloopInt ucount);
		Object(ObjectEntry* entries, bloop::BloopInt ucount, bloop::BloopInt capacity);

		union {
			struct {
				bloop::BloopChar* data;
				bloop::BloopInt len;
				bloop::BloopUInt32 hash;
			}string;
			Function* function;
			struct {
				Value* values;
				bloop::BloopInt count;
			}array;
			struct {
				ObjectEntry* entries;
				bloop::BloopInt count;
				bloop::BloopInt capacity;
			}object;
			Closure closure;
			UpValue* upvalue;
		};

		//managed by GC
		bool marked{};
		Object* next{};

		void Free();
		[[nodiscard]] std::size_t GetSize() const;

		[[nodiscard]] bool IsIndexable() const;
		[[nodiscard]] bool IsAggregate() const;
		[[nodiscard]] bool IsEqual(Object* obj);

		[[nodiscard]] bloop::BloopChar IndexChar(bloop::BloopInt idx) const;

		[[nodiscard]] Value& Index(Value vidx) const;

		[[nodiscard]] bloop::BloopString ValueToString() const;
		[[nodiscard]] bloop::BloopString TypeToString() const;

		Value& ObjectGet(Value key) const;
		Value& ObjectSet(Value key, Value value);

	private:
		[[nodiscard]] bloop::BloopString ValueToStringInternal(std::unordered_set<const Object*>& seen) const;

	};

};