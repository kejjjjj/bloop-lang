#pragma once

// internal helpers shared by the minor and the major collector

#include "vm/gc/gc.hpp"
#include "vm/vm.hpp"
#include "vm/frame.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

namespace bloop::vm::detail
{
	// calls onSlot(Object*&) for every object reference stored inside the block,
	// onUpValue(UpValue*) for every upvalue referenced by a closure
	// and onBuffer(T*&) for the out of line storage of an object (which may update the pointer)
	template<typename OnSlot, typename OnUpValue, typename OnBuffer>
	inline void VisitChildren(GCHeader* h, OnSlot&& onSlot, OnUpValue&& onUpValue, OnBuffer&& onBuffer) {

		const auto visit = [&](Value& v) {
			if (v.type == Value::Type::t_object)
				onSlot(v.obj);
		};

		if (h->kind == GCKind::upvalue) {
			auto* uv = h->GetValue<UpValue>();
			// an open upvalue points into the stack, which is a root anyway
			if (uv->IsClosed())
				visit(uv->closed);
			return;
		}

		// the owner visits the contents
		if (h->kind == GCKind::buffer)
			return;

		auto* obj = h->GetValue<Object>();

		switch (obj->type) {
		case Object::Type::ot_array:
			// first, so that the values get visited at their new location
			if (!obj->ArrayHasInlineValues())
				onBuffer(obj->array.values);

			for (bloop::BloopInt i = 0; i < obj->array.count; i++)
				visit(obj->array.values[i]);
			break;
		case Object::Type::ot_object:
			// first, so that the entries get visited at their new location
			if (!obj->ObjectHasInlineEntries())
				onBuffer(obj->object.entries);

			for (bloop::BloopInt i = 0; i < obj->object.capacity; i++) {
				visit(obj->object.entries[i].key);
				visit(obj->object.entries[i].value);
			}
			break;
		case Object::Type::ot_closure:
			for (bloop::BloopUInt i = 0; i < obj->closure.numValues; i++) {
				// null while MAKE_CLOSURE is still capturing
				if (obj->closure.upvalues[i])
					onUpValue(obj->closure.upvalues[i]);
			}
			break;
		case Object::Type::ot_string:
		case Object::Type::ot_function:
		case Object::Type::ot_nativefunction:
			break;
		}
	}

	// objects keep their variable sized data right after themselves (obj + 1),
	// so the internal pointers have to be patched after the object gets copied
	inline void RelocateInternalPointers(const GCHeader* from, GCHeader* to) {

		if (to->kind == GCKind::buffer)
			return; // plain data

		if (to->kind == GCKind::upvalue) {
			const auto* o = from->GetValue<UpValue>();
			auto* n = to->GetValue<UpValue>();
			if (o->location == &o->closed)
				n->location = &n->closed;
			return;
		}

		const auto* oldTail = reinterpret_cast<const bloop::BloopByte*>(from->GetValue<Object>() + 1);
		auto* newTail = reinterpret_cast<bloop::BloopByte*>(to->GetValue<Object>() + 1);

		const auto fix = [&]<typename T>(T*& p) {
			if (reinterpret_cast<const bloop::BloopByte*>(p) == oldTail)
				p = reinterpret_cast<T*>(newTail);
		};

		auto* obj = to->GetValue<Object>();
		switch (obj->type) {
		case Object::Type::ot_string:  fix(obj->string.data); break;
		case Object::Type::ot_array:   fix(obj->array.values); break;
		case Object::Type::ot_object:  fix(obj->object.entries); break;
		case Object::Type::ot_closure: fix(obj->closure.upvalues); break;
		case Object::Type::ot_function:
		case Object::Type::ot_nativefunction:
			break;
		}
	}
}

namespace bloop::vm
{
	// calls onSlot(Object*&) for every root that references an object
	template<typename OnSlot>
	inline void GC::ForEachRoot(OnSlot&& onSlot) {

		auto& vm = *m_pVM;

		const auto visit = [&](Value& v) {
			if (v.type == Value::Type::t_object)
				onSlot(v.obj);
		};

		for (auto& v : vm.m_oStack)
			visit(v);

		for (auto& v : vm.m_oGlobals)
			visit(v);

		for (auto& v : vm.m_oGlobalChunk.m_oConstants)
			visit(v);

		for (auto& fn : vm.m_oFunctions) {
			for (auto& v : fn.chunk.m_oConstants)
				visit(v);
		}

		for (auto*& obj : m_oTempRoots) {
			if (obj)
				onSlot(obj);
		}

		// the callee gets popped off the stack before the call, so the frame keeps it alive
		for (auto& frame : vm.m_oFrames) {
			if (frame.m_eChunkType == CallFrame::ChunkType::ct_closure) {
				onSlot(frame.m_pCallee);
				frame.m_pClosure = &frame.m_pCallee->closure;
			}
		}
	}
}
