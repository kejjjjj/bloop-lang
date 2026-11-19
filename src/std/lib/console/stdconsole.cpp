#include "std/lib/console/stdconsole.hpp"
#include "std/bind.hpp"
#include "vm/value.hpp"

#include <iostream>

using namespace bloop::vm;
using namespace bloop::standard;

namespace {
	void Log(NativeArgs args) {
		bloop::BloopString str;

		for (const auto& arg : args)
			str += arg.ValueToString();

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
