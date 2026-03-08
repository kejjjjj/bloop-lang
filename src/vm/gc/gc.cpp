#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

#include "utils/fmt.hpp"

#include <ranges>
#include <iostream>
#include <functional>

using namespace bloop::vm;

constexpr auto MB = 1024 * 1024;

GC::GC(VM* vm) : m_pVM(vm), m_oMinorSpace(*this), m_oMajorSpace(*this) {
	m_oTempRoots.reserve(BLOOP_MAX_FRAMES);
}
GC::~GC() = default;

void GC::PushTempRoot(Object* obj) {
	m_oTempRoots.push_back(obj);
}
void GC::PopTempRoot(bloop::BloopUInt count) {
	while (count-- && !m_oTempRoots.empty())
		m_oTempRoots.pop_back();
}
void GC::MinorGC() {

	m_oMinorSpace.SwapSpaces();
	MinorCopyRoots(m_pVM);
	MinorScanRememberedSet();

	auto* space = m_oMinorSpace.m_pScanSpace;
	auto scan = space->m_pBegin;

	while (scan < space->m_pPtr) {
		auto* h = reinterpret_cast<GCHeader*>(scan);
		auto* obj = h->GetValue<Object>();

		Trace(&obj, &GC::TraceMinor);
		scan += sizeof(GCHeader) + h->size;
	}

}
void GC::MinorCopyRoots(VM* vm)
{
	for (UpValue*& uv = m_pOpenUpValues; uv; uv = uv->next) {
		if (uv->location == &uv->closed && uv->closed.type == Value::Type::t_object) {
			Trace(&uv->closed.obj, &GC::TraceMinor);
		}
	}

	for (auto& glob : vm->m_oGlobals) {
		if (glob.type == Value::Type::t_object)
			Trace(&glob.obj, &GC::TraceMinor);

	}

	for (auto& v : vm->m_oStack) {
		if (v.type == Value::Type::t_object)
			Trace(&v.obj, &GC::TraceMinor);

	}
	for (auto*& tempRoot : m_oTempRoots)
		Trace(&tempRoot, &GC::TraceMinor);
}
void GC::MinorScanRememberedSet() {

	for (auto** slot : m_oMinorSpace.m_oRememberedSet) {
		Trace(slot, &GC::TraceMinor);
	}

	m_oMinorSpace.m_oRememberedSet.clear();
}
void GC::MajorGC() {

	MinorGC();

	//no allocations
	if (!m_oMajorSpace.objects)
		return;

	MarkRoots(m_pVM);
	Sweep();
}
void GC::MarkRoots(VM* vm) {

	for (UpValue* uv = m_pOpenUpValues; uv; uv = uv->next) {
		TraceUpValue(uv, &GC::Mark);
	}

	for (auto& glob : vm->m_oGlobals) {
		if (glob.type == Value::Type::t_object)
			Trace(&glob.obj, &GC::Mark);
	}

	for (auto& v : vm->m_oStack) {
		if (v.type == Value::Type::t_object)
			Trace(&v.obj, &GC::Mark);
	}

	for (auto& c : vm->m_oGlobalChunk.m_oConstants) {
		if (c.type == Value::Type::t_object)
			Trace(&c.obj, &GC::Mark);
	}

	for (auto& fn : vm->m_oFunctions) {
		for (auto& c : fn.chunk.m_oConstants) {
			if (c.type == Value::Type::t_object)
				Trace(&c.obj, &GC::Mark);
		}
	}

	for (auto* tempRoot : m_oTempRoots)
		Mark(tempRoot);

}
Object* GC::Mark(Object* obj) {

	auto h = GCHeader::GetHeader(obj);
	assert(h);
	if (!h || h->marked)
		return obj;
	h->marked = true;

	Trace(&obj, &GC::Mark);
	return obj;
}
void GC::TraceUpValue(UpValue* uv, Object*(GC::* cb)(Object*)) {

	if (!uv) //GC was called when allocating upvalues
		return;

	assert(uv != nullptr && "Null upvalue pointer passed to MarkUpValue");
	assert(uv->location != nullptr && "UpValue has null location � dangling or uninitialized?");

	if (uv->location == &uv->closed) {
		assert(uv->closed.type != Value::Type::t_undefined && "Closed upvalue contains uninitialized/none value");
		if (uv->closed.type == Value::Type::t_object) {
			assert(uv->closed.obj != nullptr && "Closed upvalue points to null object");
			assert(GCHeader::GetHeader(uv->closed.obj) != nullptr && "Closed object has no GC header");
			Trace(&uv->closed.obj, cb);
		}
	}
}
void GC::Sweep() {
	auto** obj = &m_oMajorSpace.objects;

	while (*obj) {
		GCHeader* current = *obj;
		assert(current != nullptr && "Null pointer found in object list");
		assert(current->next != current && "Self-loop in GC list detected");

		if (!current->marked) {
			auto* unreached = current;
			*obj = unreached->next;  // unlink before freeing

			assert(unreached->size > 0 && "Zero-size object being swept");
		} else {
			current->marked = false; 
			obj = &current->next;
		}
	}
}
void GC::Trace(Object** slot, Object*(GC::* cb)(Object*)) {

	if (!slot || !*slot)
		return;

	*slot = (this->*cb)(*slot);

	auto* obj = *slot;
	if (!obj) 
		return;

	switch (obj->type) {
	case Object::Type::ot_array:
		for (auto i : std::views::iota(0, obj->array.count)) {
			if (obj->array.values[i].type == Value::Type::t_object) 
				Trace(&obj->array.values[i].obj, cb);
		}
		break;

	case Object::Type::ot_object:
		for (auto i : std::views::iota(0, obj->object.capacity)) {
			auto& entry = obj->object.entries[i];
			if (entry.key.type == Value::Type::t_object)
				Trace(&entry.key.obj, cb);
			if (entry.value.type == Value::Type::t_object)
				Trace(&entry.value.obj, cb);
		}
		break;

	case Object::Type::ot_closure:
		for (auto i : std::views::iota(0u, obj->closure.numValues)) {
			auto* uv = obj->closure.upvalues[i];
			TraceUpValue(uv, cb);
		}
		break;

	default:
		break;
	}

	return;
}
Object* GC::TraceMinor(Object* obj) {

	assert(obj);
	auto h = GCHeader::GetHeader(obj);
	assert(h);

	// already copied
	if (h->forwarding)
		return obj;

	return m_oMinorSpace.Copy(h);
}
