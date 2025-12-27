#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

#include <ranges>

using namespace bloop::vm;

void GC::Collect(VM* vm) {

	//no allocations
	if (!m_pHeap->m_pObjects)
		return;

	MarkRoots(vm);
	Sweep();
	m_pHeap->m_uNextGCLimit = m_pHeap->m_uBytesAllocated * 2;
}
void GC::MarkRoots(VM* vm) {

	for (auto& glob : vm->m_oGlobals) {
		if (glob.type == Value::Type::t_object)
			Mark(glob.obj);
	}

	for (auto& v : vm->m_oStack) {
		if (v.type == Value::Type::t_object)
			Mark(v.obj);
	}

	for (const auto& frame : vm->m_oFrames) {
		for (auto& c : frame.m_pChunk->m_oConstants) {
			if (c.type == Value::Type::t_object)
				Mark(c.obj);
		}
	}

}
void GC::Mark(Object* obj) {
	if (!obj || obj->marked)
		return;
	obj->marked = true;
	Trace(obj);
}
void GC::Sweep() {
	Object** obj = &m_pHeap->m_pObjects;

	while (*obj) {
		if (!(*obj)->marked) {
			Object* unreached = *obj;
			*obj = unreached->next;
			m_pHeap->FreeObject(unreached);
		} else {
			(*obj)->marked = false; // reset flags to avoid false positives
			obj = &(*obj)->next;
		}
	}
}
void GC::Trace([[maybe_unused]] Object* obj) {

	switch (obj->type) {
	case Object::Type::ot_array: 
		for (const auto i : std::views::iota(0, obj->array.count)) {
			if (obj->array.values[i].type == Value::Type::t_object)
				Mark(obj->array.values[i].obj);
		}
		break;
	}

}