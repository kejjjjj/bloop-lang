#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

#include <ranges>

using namespace bloop::vm;

GC::GC(VM* vm) : m_pVM(vm) {
	m_oTempRoots.reserve(BLOOP_MAX_FRAMES);
}

void GC::PushTempRoot(Object* obj) {
	m_oTempRoots.push_back(obj);
}
void GC::PopTempRoot(bloop::BloopUInt count) {
	while (count-- && !m_oTempRoots.empty())
		m_oTempRoots.pop_back();
}

void GC::Collect() {

	//no allocations
	if (m_bIsPaused || !m_pObjects)
		return;

	MarkRoots(m_pVM);
	Sweep();

	if(m_uBytesAllocated >= m_uNextGCLimit)
		m_uNextGCLimit = m_uBytesAllocated * 2;
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
void GC::Mark(Object* _obj) {

	auto obj = GCHeader::GetHeader(_obj);

	if (!obj || obj->marked)
		return;
	obj->marked = true;
	Trace(_obj);
}
void GC::MarkUpValue(UpValue* uv) {
	assert(uv && uv->location);
	if (uv && uv->location == &uv->closed && uv->location->type == Value::Type::t_object)
		Mark(uv->closed.obj);
}
void GC::Sweep() {
	auto** obj = &m_pObjects;

	while (*obj) {
		if (!(*obj)->marked) {
			auto* unreached = *obj;
			*obj = unreached->next;
			if (unreached->is_object)
				FreeObject(unreached->GetValue<Object>());
			else
				FreeOther(unreached);

			delete unreached;

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
			MarkUpValue(obj->closure.upvalues[i]);
		}
		break;
	}


}

void GC::FreeObject(Object* obj) {
	assert(m_uBytesAllocated >= obj->GetSize());
	m_uBytesAllocated -= obj->GetSize();
	obj->Free();
}
void GC::FreeOther(GCHeader* header) {
	assert(m_uBytesAllocated >= header->size);
	m_uBytesAllocated -= header->size;
}