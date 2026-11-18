#include "std/native.hpp"
#include "std/bind.hpp"
#include "vm/vm.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/heap/dvalue.hpp"

#include "lib/console/stdconsole.hpp"

using namespace bloop::standard;
using namespace bloop::vm;

namespace {
	void MajorGC(VM& vm) { vm.MajorGC(); }
	void MinorGC(VM& vm) { vm.MinorGC(); }

	bloop::BloopInt Length(std::variant<StringArg, ArrayArg> value) {
		if (const auto* str = std::get_if<StringArg>(&value))
			return str->Length();
		return std::get<ArrayArg>(value).Count();
	}
}

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
			NativeField(BLOOPTEXT("length"), Bind<&Length>()),
			NativeField(BLOOPTEXT("gc_major"), Bind<&MajorGC>()),
			NativeField(BLOOPTEXT("gc_minor"), Bind<&MinorGC>())
		}
	});
}
