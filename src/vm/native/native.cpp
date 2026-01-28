#include "vm/native/native.hpp"
#include "vm/vm.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

using namespace bloop::vm;

#define FWD_DECLARE_NATIVE(Name) [[nodiscard]] Value Name(VM& vm, const std::vector<Value>& args)
#define DEFINE_NATIVE(Name, vm, args) Value Name([[maybe_unused]]VM& vm, [[maybe_unused]]const std::vector<Value>& args)

FWD_DECLARE_NATIVE(Length);

std::vector<native::NativeDef> native::g_Natives = {
	{ "length", 1u, Length }
};

DEFINE_NATIVE(Length, vm, args) {

	const auto& val = args[0];
	if (val.IsArray())
		return Value(val.obj->array.count);
	if (val.IsString())
		return Value(val.obj->string.len);

	throw bloop::exception::VMError(bloop::fmt::format("length() expects \"array\" or \"string\", but got \"{}\"", val.TypeToString()));
}
