#include "std/native.hpp"
#include "vm/vm.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

#include "lib/console/stdconsole.hpp"

using namespace bloop::standard;
using namespace bloop::vm;

FWD_DECLARE_NATIVE(Length);
FWD_DECLARE_NATIVE(RunGC);

NativeObject::~NativeObject() = default;

NativeDef::NativeDef(const bloop::BloopString& name, const NativeObject& obj)
: m_sName(name), m_eType(NativeType::object), m_oData(std::make_shared<NativeObject>(obj)) {}

std::vector<NativeDef> bloop::standard::g_Natives = {
	{ IncludeStandardLibrary() }
};

NativeDef bloop::standard::IncludeStandardLibrary() {

	return NativeDef(BLOOPTEXT("std"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("console"), bloop::standard::GetConsoleDefinitions()),
			NativeField(BLOOPTEXT("length"), { 1u, Length }),
			NativeField(BLOOPTEXT("gc_run"), { 0u, RunGC })
		}
	});
}

DEFINE_NATIVE(RunGC, vm, args) {
	vm.RunGC();
	return {};
}

DEFINE_NATIVE(Length, vm, args) {

	const auto& val = args[0];
	if (val.IsArray())
		return Value(val.obj->array.count);
	if (val.IsString())
		return Value(val.obj->string.len);
	
	throw bloop::exception::VMError(bloop::fmt::format("length() expects \"array\" or \"string\", but got \"{}\"", val.TypeToString()));
}
