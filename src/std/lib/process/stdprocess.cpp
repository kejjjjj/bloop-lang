#include "std/lib/process/stdprocess.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// std.process: the program that runs the script

namespace {

	// the arguments that the host gave to the script (the standalone interpreter: what comes after the script's path)
	Value Args(VM& vm) {
		const auto& args = vm.GetEnvironment().m_oArguments;

		auto& heap = vm.GetHeap();
		RootScope scope(vm.GetGC());
		const auto result = scope.Keep(heap.AllocArray(args.size()));

		for (std::size_t i = 0; i < args.size(); i++) {
			auto* arg = heap.AllocString(args[i].data(), args[i].size());
			result->array.values[i] = Value{ arg };
			vm.GetGC().WriteBarrier(result.Obj(), arg);
		}
		return result.Get();
	}

	// the value of an environment variable, or undefined if it isn't set
	std::optional<bloop::BloopString> Env(StringArg name) {
		const auto str = name.View();
		if (str.empty() || str.find('\0') != bloop::BloopStringView::npos || str.find('=') != bloop::BloopStringView::npos)
			Fail(BLOOPTEXT("std.process.env: invalid variable name"));

#ifdef _WIN32
		// the wide api, so that values are utf-8 (like every other string)
		const auto wideName = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(str.data()), str.size())).wstring();
		wchar_t* value{};
		std::size_t length{};
		if (_wdupenv_s(&value, &length, wideName.c_str()) != 0 || !value)
			return std::nullopt;

		const auto utf8 = std::filesystem::path(value).u8string();
		std::free(value);
		return bloop::BloopString(reinterpret_cast<const char*>(utf8.data()), utf8.size());
#else
		const auto* value = std::getenv(bloop::BloopString(str).c_str());
		if (!value)
			return std::nullopt;
		return bloop::BloopString(value);
#endif
	}

	// ends the script. The standalone interpreter exits with `code` (0 if it's left out)
	void Exit(VM& vm, std::optional<bloop::BloopInt> code) {
		vm.Exit(code.value_or(0));
	}

	constexpr auto Platform() {
#if defined(_WIN32)
		return "windows";
#elif defined(__APPLE__)
		return "macos";
#elif defined(__linux__)
		return "linux";
#else
		return "unknown";
#endif
	}
}

NativeDef bloop::standard::GetProcessDefinitions() {
	return NativeDef(BLOOPTEXT("process"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("args"), Bind<&Args>()),
			NativeField(BLOOPTEXT("env"), Bind<&Env>()),
			NativeField(BLOOPTEXT("exit"), Bind<&Exit>()),
			NativeField(BLOOPTEXT("platform"), MakeConstant(Platform())),
		}
	});
}
