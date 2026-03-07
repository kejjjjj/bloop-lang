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

	auto scan = m_oMinorSpace.m_oToSpace.m_pBegin;

	while (scan < m_oMinorSpace.m_oToSpace.m_pPtr) {
		auto* h = reinterpret_cast<GCHeader*>(scan);
		auto* obj = h->GetValue<Object>();

		Trace(obj, &GC::TraceMinor);
		scan += sizeof(GCHeader) + h->size;
	}

}
void GC::MinorCopyRoots(VM* vm)
{
	for (UpValue*& uv = m_pOpenUpValues; uv; uv = uv->next) {
		if (uv->location == &uv->closed && uv->closed.type == Value::Type::t_object) {
			uv->closed.obj = reinterpret_cast<Object*>(TraceMinor(uv->closed.obj));
		}
	}

	for (auto& v : vm->m_oStack) {
		if (v.type == Value::Type::t_object)
			v.obj = reinterpret_cast<Object*>(TraceMinor(v.obj));
	}
	for (auto*& tempRoot : m_oTempRoots)
		tempRoot = reinterpret_cast<Object*>(TraceMinor(tempRoot));

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
void* GC::Mark(Object* _obj) {

	auto obj = GCHeader::GetHeader(_obj);
	assert(obj);
	if (!obj || obj->marked)
		return nullptr;
	obj->marked = true;

	Trace(_obj, &GC::Mark);
	return nullptr;
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

	assert(m_oMajorSpace.objects != nullptr || (m_uBytesAllocated == 0 && "m_pObjects is null but bytes still allocated"));

	auto** obj = &m_oMajorSpace.objects;

	while (*obj) {
		GCHeader* current = *obj;
		assert(current != nullptr && "Null pointer found in object list");
		assert(current->next != current && "Self-loop in GC list detected");

		if (!current->marked) {
			auto* unreached = current;
			*obj = unreached->next;  // unlink before freeing

			assert(unreached->size > 0 && "Zero-size object being swept");
			assert(m_uBytesAllocated >= unreached->size && "Bytes allocated underflow during sweep");

			FreeOther(unreached);
		} else {
			current->marked = false; 
			obj = &current->next;
		}
	}
}
void GC::Trace(Object* obj, void*(GC::* cb)(Object*)) {

	assert(obj != nullptr && "Trace called on null object");
	[[maybe_unused]] const auto _header = GCHeader::GetHeader(obj);
	assert(_header != nullptr && "Object has no GC header");

	switch (obj->type) {
	case Object::Type::ot_array: 
		for (const auto i : std::views::iota(0, obj->array.count)) {
			if (obj->array.values[i].type == Value::Type::t_object)
				(this->*cb)(obj->array.values[i].obj);
		}
		break;
	case Object::Type::ot_object:
		for (const auto i : std::views::iota(0, obj->object.capacity)) {
			const auto& entry = obj->object.entries[i];
			assert(entry.key.type == Value::Type::t_object || entry.key.type == Value::Type::t_undefined);
			if (entry.key.type == Value::Type::t_object)
				(this->*cb)(entry.key.obj);
			if (entry.value.type == Value::Type::t_object)
				(this->*cb)(entry.value.obj);
		}
		break;
	case Object::Type::ot_closure:
		assert(obj->closure.upvalues != nullptr || (obj->closure.numValues == 0 && "Closure has null upvalues array but numValues > 0"));

		assert(obj->closure.numValues <= 1024 && "Suspiciously large number of upvalues");

		for (const auto i : std::views::iota(0u, obj->closure.numValues)) {
			auto* uv = obj->closure.upvalues[i];
			
			if (!uv)
				continue;

			auto header = GCHeader::GetHeader(uv);
			header->marked = true;

			if (uv->location == &uv->closed) {
				assert(uv->closed.type != Value::Type::t_undefined && "Closed upvalue contains uninitialized/none value");
				if (uv->closed.type == Value::Type::t_object) {
					assert(uv->closed.obj != nullptr && "Closed upvalue points to null object");
					assert(GCHeader::GetHeader(uv->closed.obj) != nullptr && "Closed object has no GC header");
					(this->*cb)(uv->closed.obj);
				}
			}
		}
		break;
	default:
		break;
	}

	return;
}
void* GC::TraceMinor(Object* obj) {

	assert(obj);
	auto h = GCHeader::GetHeader(obj);
	assert(h);

	//no need to move old objects
	if (h->age >= PROMOTION_THRESHOLD)
		return h->GetValue<Object>();

	// already copied
	if (h->forwarding)
		return h->GetValue<Object>();

	auto copy = m_oMinorSpace.Copy(h);
	Trace(copy, &GC::TraceMinor);

	return copy;
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
	assert(m_uBytesAllocated >= header->size && "Bytes allocated underflow in FreeOther");

	SubtractExternalBytes(sizeof(GCHeader) + header->size);
	assert(m_uBytesAllocated <= m_uBytesAllocated + header->size && "Underflow after subtracting non-object size");
}