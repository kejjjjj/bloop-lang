#include "std/lib/console/stdconsole.hpp"
#include "std/bind.hpp"
#include "vm/value.hpp"
#include "vm/vm.hpp"

#include <iostream>

using namespace bloop::vm;
using namespace bloop::standard;

namespace {
	void Log(VM& vm, NativeArgs args) {
		bloop::BloopString str;

		for (const auto& arg : args)
			str += arg.ValueToString();

		if (const auto& print = vm.GetEnvironment().m_fnPrint)
			print(str);
		else
			std::cout << str << '\n';
	}
}

bloop::standard::NativeDef bloop::standard::GetConsoleDefinitions() {

	return NativeDef(BLOOPTEXT("console"),
		NativeObject{
			.m_oFields = {
				NativeField(BLOOPTEXT("log"), Bind<&Log>()),
				NativeField(BLOOPTEXT("prop"), MakeConstant(bloop::BloopInt{ 1 }))
			}
		}
	);

}
