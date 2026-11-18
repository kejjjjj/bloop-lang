#pragma once

#include "std/native.hpp"

namespace bloop::vm {
	struct Value;
	struct Object;
	class Heap;
	class Local;
}

namespace bloop::standard {
	// creates the field's value and assigns it to the (rooted) receiver object
	void AssignField(bloop::vm::Heap& heap, const NativeField& field, const bloop::vm::Local& receiver);

	[[nodiscard]] bloop::vm::Object* FromDefinitionToObject(bloop::vm::Heap& heap, const NativeDef& def);

}