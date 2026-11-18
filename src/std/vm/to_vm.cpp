#include "std/vm/to_vm.hpp"
#include "vm/heap/heap.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/gc/handles.hpp"

#include <cassert>

using namespace bloop::vm;

// every allocation can move young objects, so the receiver and the key stay rooted
void bloop::standard::AssignField(Heap& heap, const NativeField& field, const Local& receiver) {

	RootScope scope(heap.GetGC());
	const auto key = scope.Keep(heap.AllocString(field.m_sName.data(), field.m_sName.size()));

	Value value;

	switch (field.m_eType) {
	case NativeObjectPropertyType::method:
		value = Value{ heap.AllocNativeFunction(&std::get<0>(field.m_oData)) };
		break;
	case NativeObjectPropertyType::property: {
		const auto& constant = std::get<1>(field.m_oData);

		if (std::get<1>(constant) == bloop::EValueType::t_string) {
			const auto& data = std::get<0>(constant);
			value = Value{ heap.AllocString(data.data(), data.size()) };
		} else {
			value = Value{ constant };
		}
		break;
	} 
	case NativeObjectPropertyType::native_object: 
		value = Value{ FromDefinitionToObject(heap, std::get<2>(field.m_oData)) };
		break;
	default:
		break;
	}

	// value is the newest allocation, nothing can move it before it's stored
	heap.ObjectSet(receiver.Obj(), key.Get(), value);
}

Object* bloop::standard::FromDefinitionToObject(Heap& heap, const NativeDef& def) {

	switch (def.m_eType) {
	case NativeType::function:
		return heap.AllocNativeFunction(&std::get<0>(def.m_oData));
	case NativeType::object: {
		const auto& defObj = std::get<1>(def.m_oData);
		assert(defObj);

		RootScope scope(heap.GetGC());
		const auto obj = scope.Keep(heap.AllocObject(static_cast<bloop::BloopIndex>(defObj->m_oFields.size())));
		
		for (const auto& field : defObj->m_oFields)
			AssignField(heap, field, obj);

		return obj.Obj();
	}
	default:
		return nullptr;
	}

}
