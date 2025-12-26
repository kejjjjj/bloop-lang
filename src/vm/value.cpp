#include "vm/value.hpp"
#include "vm/exception.hpp"
#include "vm/heap/dvalue.hpp"
#include "utils/fmt.hpp"


#include <unordered_map>
#include <cassert>

using namespace bloop::vm;

Value::Value(bloop::EValueType t, const bloop::BloopString& data) {

	using VT = bloop::EValueType;

	switch (t) {
	case VT::t_undefined:
		type = Type::t_undefined;
		break;
	case VT::t_boolean:
		type = Type::t_bool;
		b = static_cast<bloop::BloopBool>(data[0]);
		break;
	case VT::t_int:
		type = Type::t_int;
		i = *reinterpret_cast<bloop::BloopInt*>(const_cast<bloop::BloopChar*>(data.data()));
		break;
	case VT::t_uint:
		type = Type::t_uint;
		u = *reinterpret_cast<bloop::BloopUInt*>(const_cast<bloop::BloopChar*>(data.data()));
		break;
	case VT::t_double:
		type = Type::t_double;
		d = *reinterpret_cast<bloop::BloopDouble*>(const_cast<bloop::BloopChar*>(data.data()));
		break;
	}

}

using VT = Value::Type;

bool Value::IsTruthy() const
{
	switch (type) {
	case VT::t_undefined:
		return false;
	case VT::t_bool:
		return b;
	case VT::t_int:
		return i != 0;
	case VT::t_uint:
		return u != 0u;
	case VT::t_double:
		return d != 0.0;
	}

	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("value of type \"{}\" is not convertible to a boolean"), TypeToString()));
}
bool Value::IsString() const {
	return type == VT::t_object && obj->type == Object::Type::ot_string;
}

bloop::BloopString Value::TypeToString() const {
	switch (type) {
	case VT::t_undefined:
		return BLOOPTEXT("undefined");
	case VT::t_bool:
		return BLOOPTEXT("boolean");
	case VT::t_uint:
		return BLOOPTEXT("uint");
	case VT::t_int:
		return BLOOPTEXT("int");
	case VT::t_double:
		return BLOOPTEXT("double");
	case VT::t_object:
		return obj->TypeToString();
	default:
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("type \"{}\" is not convertible to a string"), TypeToString()));
	}
}

bloop::BloopString Value::ValueToString() const {

	switch (type) {
	case VT::t_undefined:
		return BLOOPTEXT("undefined");
	case VT::t_bool:
		return b ? BLOOPTEXT("true") : BLOOPTEXT("false");
	case VT::t_int:
		return std::to_string(i);
	case VT::t_uint:
		return std::to_string(u);
	case VT::t_double:
		return std::to_string(d);
	case VT::t_object:
		return obj->ValueToString();
	}
	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("value of type \"{}\" is not convertible to a string"), TypeToString()));

}