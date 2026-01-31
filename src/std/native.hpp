#pragma once

#include "utils/defs.hpp"

#include <vector>
#include <variant>
#include <memory>

#define FWD_DECLARE_NATIVE(Name) [[nodiscard]] bloop::vm::Value Name(bloop::vm::VM& vm, const std::vector<bloop::vm::Value>& args)
#define DEFINE_NATIVE(Name, vm, args) bloop::vm::Value Name([[maybe_unused]]bloop::vm::VM& vm, [[maybe_unused]]const std::vector<bloop::vm::Value>& args)

namespace bloop::vm {
	struct Value;
	class VM;
}

namespace bloop::standard {

	using NativeFn = bloop::vm::Value(*)(bloop::vm::VM&, const std::vector<bloop::vm::Value>&);

	enum class NativeType : bloop::BloopByte { function, object };
	enum class NativeObjectPropertyType : bloop::BloopByte { method, property, native_object };

	struct NativeFunction {
		bloop::BloopIndex m_uParamCount;
		NativeFn m_pFunction;
	};

	struct NativeField;

	struct NativeObject {
		std::vector<NativeField> m_oFields;
	};

	struct NativeDef {

		NativeDef(const bloop::BloopString& name, const NativeFunction& fn)
			: m_sName(name), m_eType(NativeType::function), m_oData(fn) {}
		NativeDef(const bloop::BloopString& name, const NativeObject& obj)
			: m_sName(name), m_eType(NativeType::object), m_oData(obj) {}

		bloop::BloopString m_sName;
		NativeType m_eType{ NativeType::function };
		std::variant<NativeFunction, NativeObject> m_oData;
	};

	struct NativeField {

		NativeField(const bloop::BloopString& name, const NativeFunction& fn)
			: m_sName(name), m_eType(NativeObjectPropertyType::method), m_oData(fn) {}
		NativeField(const bloop::BloopString& name, ConstantData constant)
			: m_sName(name), m_eType(NativeObjectPropertyType::property), m_oData(constant) {}
		NativeField(const bloop::BloopString& name, const NativeDef& def)
			: m_sName(name), m_eType(NativeObjectPropertyType::native_object), m_oData(def) {}

		bloop::BloopString m_sName;
		NativeObjectPropertyType m_eType{ NativeObjectPropertyType::method };
		std::variant<NativeFunction, bloop::ConstantData, NativeDef> m_oData;
	};

	[[nodiscard]] NativeDef IncludeStandardLibrary();
	extern std::vector<NativeDef> g_Natives;
	
}