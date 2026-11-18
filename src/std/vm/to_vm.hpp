#pragma once

#include "std/native.hpp"

namespace bloop::vm {
	struct Value;
	struct Object;
	class Heap;
}

namespace bloop::standard {
	// assigns the field to the object that is currently the top temp root of the GC
	void AssignField(bloop::vm::Heap& heap, const NativeField& field);

	[[nodiscard]] bloop::vm::Object* FromDefinitionToObject(bloop::vm::Heap& heap, const NativeDef& def);

}