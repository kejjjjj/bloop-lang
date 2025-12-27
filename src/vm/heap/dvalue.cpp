#include "vm/heap/dvalue.hpp"
#include "vm/value.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/vm.hpp"

#include <cassert>
#include <ranges>

using namespace bloop::vm;
using namespace std::string_literals;

Object::Object(bloop::BloopInt ucount) : type(Type::ot_array), array({ .values = new Value[ucount], .count = ucount }) {}


void Object::Free()
{
	switch (type) {
	case Type::ot_string:
		delete[] string.data;
		return;
	case Type::ot_array:
		delete[] array.values;
		return;
	case Type::ot_function:
		//just a handle
		break;
	default:
		break;
	}
}

std::size_t Object::GetSize() const
{
	switch (type) {
	case Type::ot_string:
		return sizeof(Object) + string.len + sizeof(string.len);
	case Type::ot_array:
		return sizeof(Object) + (sizeof(Value) * array.count + sizeof(array.count));
	case Type::ot_function:
		return sizeof(Object); //just a handle, has no allocated size
	default:
		return sizeof(Object);
	}
}

bool Object::IsIndexable() const {
	return type == Type::ot_array;
}
Value& Object::Index(bloop::BloopInt idx) const {
	switch (type) {
	case Type::ot_array:
		
		if (idx < 0 || idx >= array.count)
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("out of bounds index [{}]"), idx));

		return array.values[idx];
	default:
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("can't index a value of type \"{}\""), TypeToString()));
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
	std::unordered_set<const Object*> seen;
	return ValueToStringInternal(seen);
}
bloop::BloopString Object::ValueToStringInternal(std::unordered_set<const Object*>& seen) const {
	switch (type) {
	case VT::ot_string:
		return bloop::BloopString(string.data, string.len);
	case Type::ot_array: {

		if (seen.contains(this))
			return BLOOPTEXT("...");


		bloop::BloopOStringStream ss;
		for (const auto i : std::views::iota(0, array.count)) {
			if (i)
				ss << bloop::BloopString(", ");

			if (array.values[i].type != Value::Type::t_object)
				ss << array.values[i].ValueToString();
			else {
				seen.insert(this);
				ss << array.values[i].obj->ValueToStringInternal(seen);
				seen.erase(this);
			}
		}

		return BLOOPTEXT("[ ") + ss.str() + BLOOPTEXT(" ]");
	}
	case VT::ot_function:
		return BLOOPTEXT("function");

	}
	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("value of type \"{}\" is not convertible to a string"), TypeToString()));

}