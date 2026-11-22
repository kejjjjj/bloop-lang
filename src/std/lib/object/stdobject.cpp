#include "std/lib/object/stdobject.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// keys are strings or handles, and listing functions return them in insertion order

namespace {

	// an array with the key (or the value) of every property, in insertion order
	template<bool Keys>
	Value List(VM& vm, ObjectArg obj) {
		const auto slots = obj.Obj()->ObjectSlotsInOrder();

		auto* result = vm.GetHeap().AllocArray(slots.size());

		// re-read: the allocation may have moved the object (the slots stay the same, only inserting moves entries)
		const auto* entries = obj.Obj()->object.entries;
		for (std::size_t i = 0; i < slots.size(); i++)
			result->array.values[i] = Keys ? entries[slots[i]].key : entries[slots[i]].value;

		vm.GetGC().WriteBarrierAll(result);
		return Value{ result };
	}
	Value Keys(VM& vm, ObjectArg obj) { return List<true>(vm, obj); }
	Value Values(VM& vm, ObjectArg obj) { return List<false>(vm, obj); }

	bloop::BloopBool Has(ObjectArg obj, KeyArg key) {
		return obj.Obj()->ObjectFind(key.Get()) != nullptr;
	}

	// the value of the property, or undefined if it doesn't exist (obj.key would be an error)
	Value Get(ObjectArg obj, KeyArg key) {
		const auto* value = obj.Obj()->ObjectFind(key.Get());
		return value ? *value : Value{};
	}

	// adds or changes a property (unlike obj[key] = value, the key doesn't need to exist), returns the value
	Value Set(VM& vm, ObjectArg obj, KeyArg key, Value value) {
		vm.GetHeap().ObjectSet(obj.Obj(), key.Get(), value);
		return obj.Obj()->ObjectGet(key.Get()); // re-read, setting may have allocated
	}

	// removes a property, returns whether it existed
	bloop::BloopBool Remove(ObjectArg obj, KeyArg key) {
		return obj.Obj()->ObjectRemove(key.Get());
	}
}

NativeDef bloop::standard::GetObjectDefinitions() {
	return NativeDef(BLOOPTEXT("object"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("keys"), Bind<&Keys>()),
			NativeField(BLOOPTEXT("values"), Bind<&Values>()),
			NativeField(BLOOPTEXT("has"), Bind<&Has>()),
			NativeField(BLOOPTEXT("get"), Bind<&Get>()),
			NativeField(BLOOPTEXT("set"), Bind<&Set>()),
			NativeField(BLOOPTEXT("remove"), Bind<&Remove>()),
		}
	});
}
