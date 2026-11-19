#pragma once

#include "utils/defs.hpp"
#include "vm/gc/gc.hpp"

#include <unordered_set>
#include <vector>

namespace bloop::standard {
	struct NativeFunction;
}

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

	struct StringData {
		bloop::BloopChar* data;
		bloop::BloopInt len;
		bloop::BloopUInt32 hash;
	};
	
	// a hash table with linear probing, at most 75% full.
	// The entries live right after the object until it grows, then in a separate GC buffer
	struct ObjectData {
		ObjectEntry* entries;
		bloop::BloopInt count;
		bloop::BloopInt capacity;
		bloop::BloopInt nextOrder; // ObjectEntry::order of the next new key
	};

	// the values live right after the array until it grows, then in a separate GC buffer.
	// Slots in [count, capacity) are always undefined
	struct ArrayData {
		Value* values;
		bloop::BloopInt count;
		bloop::BloopInt capacity;
	};

	struct Object {
		enum class Type { ot_string, ot_array, ot_object, ot_function, ot_closure, ot_nativefunction } type;

		Object(bloop::BloopChar* _data, bloop::BloopInt _len) : type(Type::ot_string), string({.data=_data, .len=_len,.hash={}}) {}
		Object(Function* chunk) : type(Type::ot_function), function(chunk){}
		Object(Function* function, UpValue** upVals, bloop::BloopUInt numVals);

		Object(Value* values, bloop::BloopInt ucount);
		Object(ObjectEntry* entries, bloop::BloopInt ucount, bloop::BloopInt capacity);
		Object(const standard::NativeFunction* def) : type(Type::ot_nativefunction), nativeFunction(def){}

		union {
			StringData string;
			ObjectData object;
			ArrayData array;
			Closure closure;
			Function* function;
			const standard::NativeFunction* nativeFunction;
		};

		[[nodiscard]] constexpr bool IsString() const noexcept { return type == Type::ot_string; }
		[[nodiscard]] constexpr bool IsArray() const noexcept { return type == Type::ot_array; }
		[[nodiscard]] constexpr bool IsObject() const noexcept { return type == Type::ot_object; }
		[[nodiscard]] constexpr bool IsFunction() const noexcept { return type == Type::ot_function; }
		[[nodiscard]] constexpr bool IsClosure() const noexcept { return type == Type::ot_closure; }

		[[nodiscard]] bool IsIndexable() const;
		[[nodiscard]] bool IsAggregate() const;
		[[nodiscard]] bool IsEqual(Object* obj);

		[[nodiscard]] bloop::BloopChar IndexChar(bloop::BloopInt idx) const;

		[[nodiscard]] Value& Index(Value vidx) const;

		[[nodiscard]] bloop::BloopString ValueToString(bloop::BloopUInt objectIndent=0u) const;
		[[nodiscard]] bloop::BloopString TypeToString() const;

		[[nodiscard]] Value& ObjectGet(Value key) const;   // throws if the key doesn't exist
		[[nodiscard]] Value* ObjectFind(Value key) const;  // nullptr if the key doesn't exist
		[[nodiscard]] bloop::BloopInt ObjectFindSlot(Value key) const; // index into object.entries, -1 if the key doesn't exist
		// inserts without growing: the object must have room (use Heap::ObjectSet to insert new keys)
		[[maybe_unused]] Value& ObjectSet(Value key, Value value);
		[[nodiscard]] inline bool ObjectNeedsToGrow() const noexcept { return (object.count + 1) * 4 > object.capacity * 3; } // > 75% full
		// entries live right after the object until it grows, then in a separate GC buffer
		[[nodiscard]] inline bool ObjectHasInlineEntries() const noexcept { return reinterpret_cast<const void*>(object.entries) == reinterpret_cast<const void*>(this + 1); }
		// removes a key (backward shift deletion, no tombstones). Returns false if it didn't exist
		[[maybe_unused]] bool ObjectRemove(Value key);
		// the indices of the used entries, in insertion order
		[[nodiscard]] std::vector<bloop::BloopInt> ObjectSlotsInOrder() const;
		// inserts a key that doesn't exist yet into a table that has room, returns its entry
		[[nodiscard]] ObjectEntry& ObjectInsertNew(Value key);

		[[nodiscard]] inline bool ArrayHasInlineValues() const noexcept { return reinterpret_cast<const void*>(array.values) == reinterpret_cast<const void*>(this + 1); }

		[[nodiscard]] inline bool IsOld() const noexcept { return GCHeader::GetHeader(this)->IsOld(); }
		[[nodiscard]] inline bool IsYoung() const noexcept { return GCHeader::GetHeader(this)->IsYoung(); }

	private:
		[[nodiscard]] bloop::BloopString ValueToStringInternal(std::unordered_set<const Object*>& seen, bloop::BloopUInt depth=0u) const;

	};

};