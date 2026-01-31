#include "std/lib/console/stdconsole.hpp"
#include "vm/value.hpp"

#include <iostream>

using namespace bloop::vm;
using namespace bloop::standard;

FWD_DECLARE_NATIVE(Log);

bloop::standard::NativeDef bloop::standard::GetConsoleDefinitions() {

	return NativeDef(BLOOPTEXT("console"),
		NativeObject{
			.m_oFields = {
				NativeField(BLOOPTEXT("log"), { VARIADIC_PARAMETER_COUNT, Log }),
				NativeField(BLOOPTEXT("prop"), ConstantData{"\x01", EValueType::t_int})
			}
		}
	);

}

DEFINE_NATIVE(Log, vm, args) {

	bloop::BloopString str;

	for (const auto& arg : args)
		str += arg.ValueToString();

	std::cout << str << '\n';
	return Value{};
}
