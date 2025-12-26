#include "vm/heap/dvalue.hpp"
#include "vm/value.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

#include <cassert>

using namespace bloop::vm;

void Object::Free()
{
	switch (type) {
	case Type::ot_string:
		delete[] string.data;
		return;
	default:
		break;
	}
}

std::size_t Object::GetSize() const
{
	switch (type) {
	case Type::ot_string:
		return string.len;
	default:
		return 0;
	}
}
using VT = Object::Type;
bloop::BloopString Object::TypeToString() const {

	switch (type) {
	case VT::ot_string:
		return BLOOPTEXT("string");
	case VT::ot_array:
		return BLOOPTEXT("array");
	case VT::ot_object:
		return BLOOPTEXT("object");
	case VT::ot_function:
		return BLOOPTEXT("function");
	}
	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("type \"{}\" is not convertible to a string"), TypeToString()));
}

bloop::BloopString Object::ValueToString() const {

	switch (type) {
	case VT::ot_string:
		return bloop::BloopString(string.data, string.len);
	}

	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("value of type \"{}\" is not convertible to a string"), TypeToString()));

}