#include "std/vm/to_vm.hpp"
#include "vm/heap/heap.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

using namespace bloop::vm;

void bloop::standard::AssignField(Heap& heap, const NativeField& field, Object* receiver) {

	const Value key{ heap.AllocString(const_cast<char*>(field.m_sName.data()), field.m_sName.size()) };

	switch (field.m_eType) {
	case NativeObjectPropertyType::method:
		receiver->ObjectSet(key, heap.AllocNativeFunction(&std::get<0>(field.m_oData)));
		break;
	case NativeObjectPropertyType::property: {
		const auto& constant = std::get<1>(field.m_oData);

		if (std::get<1>(constant) == bloop::EValueType::t_string) {
			const auto& data = std::get<0>(constant);
			receiver->ObjectSet(key, { heap.AllocString(const_cast<char*>(data.data()), data.size()) });
		} else {
			receiver->ObjectSet(key, Value{ constant });
		}
		break;
	} 
	case NativeObjectPropertyType::native_object: 
		receiver->ObjectSet(key, Value{ FromDefinitionToObject(heap, std::get<2>(field.m_oData)) });
		break;
	default:
		break;
	}

}

Object* bloop::standard::FromDefinitionToObject(Heap& heap, const NativeDef& def) {

	switch (def.m_eType) {
	case NativeType::function:
		return heap.AllocNativeFunction(&std::get<0>(def.m_oData));
	case NativeType::object: {
		const auto& defObj = std::get<1>(def.m_oData);
		auto obj = heap.AllocObject(static_cast<bloop::BloopIndex>(defObj.m_oFields.size()));
		
		for (const auto& field : defObj.m_oFields) {
			AssignField(heap, field, obj);
		}

		return obj;
	}
	default:
		return nullptr;
	}

}
