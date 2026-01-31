#pragma once

#include "std/native.hpp"

namespace bloop::vm {
	struct Value;
	struct Object;
	class Heap;
}

namespace bloop::standard {
	[[nodiscard]] void AssignField(bloop::vm::Heap& heap, const NativeField& field, bloop::vm::Object* receiver);

	[[nodiscard]] bloop::vm::Object* FromDefinitionToObject(bloop::vm::Heap& heap, const NativeDef& def);

}