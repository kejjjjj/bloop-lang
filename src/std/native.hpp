#pragma once

#include "utils/defs.hpp"
#include "std/environment.hpp"

#include <vector>
#include <variant>
#include <memory>
#include <span>
#include <cstring>
#include <type_traits>

// args is a view into the VM stack: the arguments stay GC roots during the call and get updated if they move.
// Values read from args are only valid until the next allocation
#define FWD_DECLARE_NATIVE(Name) [[nodiscard]] bloop::vm::Value Name(bloop::vm::VM& vm, bloop::standard::NativeArgs args)
#define DEFINE_NATIVE(Name, vm, args) bloop::vm::Value Name([[maybe_unused]]bloop::vm::VM& vm, [[maybe_unused]]bloop::standard::NativeArgs args)

namespace bloop::vm {
	struct Value;
	class VM;
}

namespace bloop::standard {

	using NativeArgs = std::span<const bloop::vm::Value>;
	using NativeFn = bloop::vm::Value(*)(bloop::vm::VM&, NativeArgs);

	enum class NativeType : bloop::BloopByte { function, object };
	enum class NativeObjectPropertyType : bloop::BloopByte { method, property, native_object };

	struct NativeFunction {
		bloop::BloopIndex m_uParamCount;
		NativeFn m_pFunction;
	};

	struct NativeField;

	struct NativeObject {
		~NativeObject();
		std::vector<NativeField> m_oFields;
	};

	struct NativeDef {

		NativeDef(const bloop::BloopString& name, const NativeFunction& fn)
			: m_sName(name), m_eType(NativeType::function), m_oData(fn) {}
		NativeDef(const bloop::BloopString& name, const NativeObject& obj);

		bloop::BloopString m_sName;
		NativeType m_eType{ NativeType::function };
		std::variant<NativeFunction, std::shared_ptr<NativeObject>> m_oData;
	};

	struct NativeField {

		NativeField(const bloop::BloopString& name, const NativeFunction& fn, Permission permission = Permission::none)
			: m_sName(name), m_eType(NativeObjectPropertyType::method), m_oData(fn), m_ePermission(permission) {}
		NativeField(const bloop::BloopString& name, ConstantData constant)
			: m_sName(name), m_eType(NativeObjectPropertyType::property), m_oData(constant) {}
		NativeField(const bloop::BloopString& name, const NativeDef& def, Permission permission = Permission::none)
			: m_sName(name), m_eType(NativeObjectPropertyType::native_object), m_oData(def), m_ePermission(permission) {}

		bloop::BloopString m_sName;
		NativeObjectPropertyType m_eType{ NativeObjectPropertyType::method };
		std::variant<NativeFunction, bloop::ConstantData, NativeDef> m_oData;
		// the field only exists when the VM's environment allows this
		Permission m_ePermission{ Permission::none };
	};

	// encodes a native constant the same way the parser encodes literals
	template<typename T>
	[[nodiscard]] inline bloop::ConstantData MakeConstant(T value) {
		if constexpr (std::is_same_v<T, bloop::BloopBool>) {
			return { bloop::BloopString(1, value ? '\x01' : '\x00'), bloop::EValueType::t_boolean };
		} else if constexpr (std::is_convertible_v<T, bloop::BloopStringView>) {
			return { bloop::BloopString(bloop::BloopStringView(value)), bloop::EValueType::t_string };
		} else {
			static_assert(std::is_same_v<T, bloop::BloopInt> || std::is_same_v<T, bloop::BloopUInt> || std::is_same_v<T, bloop::BloopDouble>,
				"use BloopBool, BloopInt, BloopUInt, BloopDouble or a string");

			constexpr auto type = std::is_same_v<T, bloop::BloopInt> ? bloop::EValueType::t_int
				: std::is_same_v<T, bloop::BloopUInt> ? bloop::EValueType::t_uint : bloop::EValueType::t_double;

			bloop::BloopString data(sizeof(T), '\0');
			std::memcpy(data.data(), &value, sizeof(T));
			return { data, type };
		}
	}

	[[nodiscard]] NativeDef IncludeStandardLibrary();
	extern std::vector<NativeDef> g_Natives;
	
}