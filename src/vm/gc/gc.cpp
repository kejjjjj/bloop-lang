#include "vm/gc/gc.hpp"
#include "vm/heap/heap.hpp"
#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

#include "utils/fmt.hpp"

#include <ranges>
#include <iostream>

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
	if (!m_pObjects)
		return;

	MarkRoots(m_pVM);
	Sweep();

	if(m_uBytesAllocated >= m_uNextGCLimit)
		m_uNextGCLimit = m_uBytesAllocated * 2;
}
void GC::MarkRoots(VM* vm) {

	for (UpValue* uv = m_pOpenUpValues; uv; uv = uv->next) {
		MarkUpValue(uv);
	}

	for (auto& glob : vm->m_oGlobals) {
		if (glob.type == Value::Type::t_object)
			Mark(glob.obj);
	}

	for (auto& v : vm->m_oStack) {
		if (v.type == Value::Type::t_object)
			Mark(v.obj);
	}

	for (auto& c : vm->m_oGlobalChunk.m_oConstants) {
		if (c.type == Value::Type::t_object)
			Mark(c.obj);
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
	assert(obj);
	if (!obj || obj->marked)
		return;
	obj->marked = true;
	Trace(_obj);
}
void GC::MarkUpValue(UpValue* uv) {

	if (!uv) //GC was called when allocating upvalues
		return;

	assert(uv != nullptr && "Null upvalue pointer passed to MarkUpValue");
	assert(uv->location != nullptr && "UpValue has null location � dangling or uninitialized?");
	auto header = GCHeader::GetHeader(uv);
	assert(header != nullptr && "UpValue object has no GC header � not heap allocated?");

	header->marked = true;

	if (uv->location == &uv->closed) {
		assert(uv->closed.type != Value::Type::t_undefined && "Closed upvalue contains uninitialized/none value");
		if (uv->closed.type == Value::Type::t_object) {
			assert(uv->closed.obj != nullptr && "Closed upvalue points to null object");
			assert(GCHeader::GetHeader(uv->closed.obj) != nullptr && "Closed object has no GC header");
			Mark(uv->closed.obj);
		}
	}
}
void GC::Sweep() {

	assert(m_pObjects != nullptr || (m_uBytesAllocated == 0 && "m_pObjects is null but bytes still allocated"));

	auto** obj = &m_pObjects;

	[[maybe_unused]] bloop::BloopUInt swept_count = 0u;
	[[maybe_unused]] bloop::BloopUInt kept_count = 0u;

	while (*obj) {
		GCHeader* current = *obj;
		assert(current != nullptr && "Null pointer found in object list");
		assert(current->next != current && "Self-loop in GC list detected");

		if (!current->marked) {
			auto* unreached = current;
			*obj = unreached->next;  // unlink before freeing

			assert(unreached->size > 0 && "Zero-size object being swept");
			assert(m_uBytesAllocated >= unreached->size && "Bytes allocated underflow during sweep");

			if (unreached->is_object) {
				const auto value = unreached->GetValue<Object>();
				assert(value != nullptr && "is_object set but GetValue<Object>() returned null");
				assert(reinterpret_cast<char*>(value) - sizeof(GCHeader) == reinterpret_cast<char*>(unreached)
					&& "Header placement broken - wrong offset");

				FreeObject(value);
			} else {
				FreeOther(unreached);
			}

			//::operator delete(unreached, unreached->size + sizeof(GCHeader));
			::operator delete(unreached);
			swept_count++;
		} else {
			assert(current->marked && "Marked flag lost before sweep");
			current->marked = false; 
			obj = &current->next;
			kept_count++;
		}
	}
}
void GC::Trace(Object* obj) {

	assert(obj != nullptr && "Trace called on null object");
	[[maybe_unused]] const auto header = GCHeader::GetHeader(obj);
	assert(header != nullptr && "Object has no GC header");
	assert(header->marked && "Tracing unmarked object � should be gray/black already");

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
			assert(entry.key.type == Value::Type::t_object || entry.key.type == Value::Type::t_undefined);
			if (entry.key.type == Value::Type::t_object)
				Mark(entry.key.obj);
			if (entry.value.type == Value::Type::t_object)
				Mark(entry.value.obj);
		}
		break;
	case Object::Type::ot_closure:
		assert(obj->closure.upvalues != nullptr || (obj->closure.numValues == 0 && "Closure has null upvalues array but numValues > 0"));

		assert(obj->closure.numValues <= 1024 && "Suspiciously large number of upvalues");

		for (const auto i : std::views::iota(0u, obj->closure.numValues)) {
			auto* uv = obj->closure.upvalues[i];
			MarkUpValue(uv);
		}
		break;
	default:
		break;
	}


}

void GC::FreeObject(Object* obj) {

	SubtractExternalBytes(sizeof(GCHeader) + sizeof(Object));
	const auto external = obj->GetExternalBytes();

	if (external > 0) {
		SubtractExternalBytes(external);
		assert(m_uBytesAllocated <= m_uBytesAllocated + external);
	}

	obj->Free();
}
void GC::FreeOther(GCHeader* header) {
	assert(header != nullptr && "FreeOther called with null header");
	assert(header->size > 0 && "Zero-size non-object being freed");
	assert(!header->is_object && "is_object flag set on non-object header");
	assert(m_uBytesAllocated >= header->size && "Bytes allocated underflow in FreeOther");

	SubtractExternalBytes(sizeof(GCHeader) + header->size);
	assert(m_uBytesAllocated <= m_uBytesAllocated + header->size && "Underflow after subtracting non-object size");
}