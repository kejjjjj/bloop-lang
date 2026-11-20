#include "std/native.hpp"
#include "std/bind.hpp"
#include "vm/vm.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/heap/dvalue.hpp"

#include "lib/console/stdconsole.hpp"
#include "lib/array/stdarray.hpp"
#include "lib/string/stdstring.hpp"
#include "lib/object/stdobject.hpp"
#include "lib/coroutine/stdcoroutine.hpp"
#include "lib/time/stdtime.hpp"
#include "lib/math/stdmath.hpp"
#include "lib/json/stdjson.hpp"
#include "lib/fs/stdfs.hpp"
#include "lib/process/stdprocess.hpp"

using namespace bloop::standard;
using namespace bloop::vm;

namespace {
	void MajorGC(VM& vm) { vm.MajorGC(); }
	void MinorGC(VM& vm) { vm.MinorGC(); }

	// the number of characters (bytes) of a string, elements of an array or properties of an object
	bloop::BloopInt Length(std::variant<StringArg, ArrayArg, ObjectArg> value) {
		if (const auto* str = std::get_if<StringArg>(&value))
			return str->Length();
		if (const auto* arr = std::get_if<ArrayArg>(&value))
			return arr->Count();
		return std::get<ObjectArg>(value).Obj()->object.count;
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
			NativeField(BLOOPTEXT("array"), bloop::standard::GetArrayDefinitions()),
			NativeField(BLOOPTEXT("string"), bloop::standard::GetStringDefinitions()),
			NativeField(BLOOPTEXT("object"), bloop::standard::GetObjectDefinitions()),
			NativeField(BLOOPTEXT("json"), bloop::standard::GetJsonDefinitions()),
			NativeField(BLOOPTEXT("coroutine"), bloop::standard::GetCoroutineDefinitions()),
			NativeField(BLOOPTEXT("time"), bloop::standard::GetTimeDefinitions()),
			NativeField(BLOOPTEXT("math"), bloop::standard::GetMathDefinitions()),
			// these reach outside of the VM, so they only exist when the VM's environment allows them
			NativeField(BLOOPTEXT("fs"), bloop::standard::GetFileSystemDefinitions(), Permission::file_system),
			NativeField(BLOOPTEXT("process"), bloop::standard::GetProcessDefinitions(), Permission::process),
			NativeField(BLOOPTEXT("length"), Bind<&Length>()),
			NativeField(BLOOPTEXT("gc_major"), Bind<&MajorGC>()),
			NativeField(BLOOPTEXT("gc_minor"), Bind<&MinorGC>())
		}
	});
}
