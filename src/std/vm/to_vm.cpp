#include "std/vm/to_vm.hpp"
#include "vm/heap/heap.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

#include <cassert>

using namespace bloop::vm;

// Every allocation can move young objects, so the receiver and the key are kept in temp roots
// and re-read after the allocations are done.
void bloop::standard::AssignField(Heap& heap, const NativeField& field) {

	auto& gc = heap.GetGC();

	// receiver is TempRoot(0) on entry
	gc.PushTempRoot(heap.AllocString(field.m_sName.data(), field.m_sName.size()));

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

	// no allocations below this line
	auto* key = gc.TempRoot(0u);
	auto* receiver = gc.TempRoot(1u);
	gc.PopTempRoot();

	receiver->ObjectSet(Value{ key }, value);
	gc.WriteBarrier(receiver, key);
	gc.WriteBarrier(receiver, value);
}

Object* bloop::standard::FromDefinitionToObject(Heap& heap, const NativeDef& def) {

	switch (def.m_eType) {
	case NativeType::function:
		return heap.AllocNativeFunction(&std::get<0>(def.m_oData));
	case NativeType::object: {
		const auto& defObj = std::get<1>(def.m_oData);
		assert(defObj);

		auto& gc = heap.GetGC();
		gc.PushTempRoot(heap.AllocObject(static_cast<bloop::BloopIndex>(defObj->m_oFields.size())));
		
		for (const auto& field : defObj->m_oFields) {
			AssignField(heap, field);
		}

		auto* obj = gc.TempRoot();
		gc.PopTempRoot();
		return obj;
	}
	default:
		return nullptr;
	}

}
