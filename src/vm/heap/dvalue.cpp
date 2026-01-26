#include "vm/heap/dvalue.hpp"
#include "vm/value.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/vm.hpp"

#include <cassert>
#include <ranges>

using namespace bloop::vm;
using namespace std::string_literals;
Object::Object(Function* function, UpValue** upVals, bloop::BloopUInt numVals) 
	: type(Type::ot_closure), closure({ .function = function, .upvalues = upVals, .numValues= numVals }) {}

Object::Object(Value* values, bloop::BloopInt ucount) : type(Type::ot_array), array({ .values = values, .count = ucount }) {}
Object::Object(ObjectEntry* entries, bloop::BloopInt ucount, bloop::BloopInt capacity) 
	: type(Type::ot_object), object({ .entries = entries, .count = ucount, .capacity = capacity }) {}

void Object::Free()
{
	switch (type) {
	case Type::ot_string:
		delete[] string.data;
		return;
	case Type::ot_array:
		delete[] array.values;
		return;
	case Type::ot_object:
		delete[] object.entries;
		return;
	case Type::ot_function:
		//just a handle
		break;
	case Type::ot_closure:
		delete[] closure.upvalues;
		break;
	default:
		break;
	}
}

std::size_t Object::GetSize() const
{
	switch (type) {
	case Type::ot_string:
		return sizeof(Object) + string.len + sizeof(string.hash);
	case Type::ot_array:
		return sizeof(Object) + (sizeof(array.values) * array.count);
	case Type::ot_object:
		return sizeof(Object) + sizeof(*object.entries) + sizeof(object.count) + sizeof(object.capacity);
	case Type::ot_function:
		return sizeof(Object) + sizeof(function); //just a handle, has no allocated size
	case Type::ot_closure:
		return sizeof(Object) + (sizeof(closure.upvalues) * closure.numValues);
	default:
		return sizeof(Object);
	}
}

bool Object::IsIndexable() const {
	return type == Type::ot_array || type == Type::ot_object || type == Type::ot_string;
}
bool Object::IsAggregate() const {
	return type == Type::ot_object;
}
bool Object::IsEqual(Object* obj)
{
	switch (type) {
	case Type::ot_string:
		if (string.hash != obj->string.hash)
			return false;
		if (string.len != obj->string.len)
			return false;
		return !memcmp(string.data, obj->string.data, string.len);
	default:
		return this == obj;
	}
}
bloop::BloopChar Object::IndexChar(bloop::BloopInt idx) const {
	if (idx < 0 || idx >= string.len)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("out of bounds index [{}]"), idx));

	return string.data[idx];
}
Value& Object::Index(Value vidx) const {
	switch (type) {
	case Type::ot_array: {
		const auto idx = vidx.ToInt();

		if (idx < 0 || idx >= array.count)
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("out of bounds index [{}]"), idx));

		return array.values[idx];
	} case Type::ot_object: {

		if (!vidx.IsString())
			throw exception::VMError(BLOOPTEXT("object index must be a string"));

		return ObjectGet(vidx);
	} default:
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("can't use an index of type \"{}\" for this operation"), TypeToString()));
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
	case VT::ot_closure:
		return BLOOPTEXT("closure");
	}

	throw exception::VMError(BLOOPTEXT("type is not convertible to a string"));
}

bloop::BloopString Object::ValueToString(bloop::BloopUInt objectIndent) const {
	std::unordered_set<const Object*> seen;
	return ValueToStringInternal(seen, objectIndent);
}
bloop::BloopString Object::ValueToStringInternal(std::unordered_set<const Object*>& seen, bloop::BloopUInt depth) const {
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
	} case VT::ot_object: {

		if (seen.contains(this))
			return BLOOPTEXT("...");

		auto oldIndent = bloop::BloopString(depth, BLOOPTEXT(' '));
		auto newIndent = bloop::BloopString(depth + 2u, BLOOPTEXT(' '));

		bloop::BloopOStringStream ss;

		for (bloop::BloopUInt count{}; const auto i : std::views::iota(0, object.capacity)) {

			if (object.entries[i].key.type == Value::Type::t_undefined)
				continue;

			if (count)
				ss << BLOOPTEXT(",\n");

			seen.insert(this);

			const auto key = object.entries[i].key.ValueToString(depth + 2u);

			const auto value = object.entries[i].value.type != Value::Type::t_object ?
				object.entries[i].value.ValueToString(depth + 1u) :
				object.entries[i].value.obj->ValueToStringInternal(seen, depth + 2u);

			ss << bloop::fmt::format(BLOOPTEXT("{}\"{}\": {}"), newIndent, key, value);

			count++;
			seen.erase(this);
		}

		return bloop::fmt::format(BLOOPTEXT("{\n{}\n{}}"), ss.str(), oldIndent);
	} case VT::ot_function:
		return BLOOPTEXT("function");
	case VT::ot_closure:
		return BLOOPTEXT("closure");
	}
	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("value of type \"{}\" is not convertible to a string"), TypeToString()));

}
Value& Object::ObjectGet(Value key) const {

	assert((object.capacity & (object.capacity - 1)) == 0); // power of 2

	const auto mask = object.capacity - 1;
	auto index = key.Hash() & mask;

	for (;;) {
		auto& entry = object.entries[index];

		if (entry.key.type == Value::Type::t_undefined) {
			throw exception::VMError(bloop::fmt::format(BLOOPTEXT("unknown property \"{}\""), key.ValueToString()));
		}

		if (key.IsEqual(entry.key)) {
			return entry.value;
		}

		index = (index + 1) & mask;
	}
}

Value& Object::ObjectSet(Value key, Value value)
{
	assert((object.capacity & (object.capacity - 1)) == 0); // power of 2
	const auto mask = object.capacity - 1;
	auto index = key.Hash() & mask;

	for (;;) {

		auto& entry = object.entries[index];

		if (entry.key.type == Value::Type::t_undefined) {
			entry.key = key;
			entry.value = value;
			object.count++;
			return entry.value;
		}

		if (entry.key.IsEqual(key)) {
			entry.value = value;
			return entry.value;
		}

		index = (index + 1) & mask;
	}

}