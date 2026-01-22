#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

#include <ranges>

using namespace bloop::vm;

GC::GC(Heap* heap) : m_pHeap(heap) {
	m_oTempRoots.reserve(BLOOP_MAX_FRAMES);
}

void GC::PushTempRoot(Object* obj) {
	m_oTempRoots.push_back(obj);
}
void GC::PopTempRoot(bloop::BloopUInt count) {
	while (count-- && !m_oTempRoots.empty())
		m_oTempRoots.pop_back();
}

void GC::Collect(VM* vm) {

	//no allocations
	if (m_bIsPaused || !m_pHeap->m_pObjects)
		return;

	MarkRoots(vm);
	Sweep();

	if(m_pHeap->m_uBytesAllocated >= m_pHeap->m_uNextGCLimit)
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

	for (auto& fn : vm->m_oFunctions) {
		for (auto& c : fn.chunk.m_oConstants) {
			if (c.type == Value::Type::t_object)
				Mark(c.obj);
		}
	}

	for (auto* tempRoot : m_oTempRoots)
		Mark(tempRoot);

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
void GC::Trace(Object* obj) {

	switch (obj->type) {
	case Object::Type::ot_array: 
		for (const auto i : std::views::iota(0, obj->array.count)) {
			if (obj->array.values[i].type == Value::Type::t_object)
				Mark(obj->array.values[i].obj);
		}
		break;
	case Object::Type::ot_object:
		for (const auto i : std::views::iota(0, obj->object.capacity)) {
			const auto& entry = obj->object.entries[i];
			if (entry.key.type == Value::Type::t_object)
				Mark(entry.key.obj);
			if (entry.value.type == Value::Type::t_object)
				Mark(entry.value.obj);
		}
		break;
	case Object::Type::ot_closure:
		for (const auto i : std::views::iota(0u, obj->closure.numValues)) {
			if (obj->closure.upvalues[i]->location->type == Value::Type::t_object)
				Mark(obj->closure.upvalues[i]->location->obj);
		}
		break;
	}


}