#pragma once

// bloop.hpp: running bloop scripts from a C++ program (a game that loads mods, for example).
//
//     #include "bloop.hpp"
//
//     bloop::ScriptOptions options;
//     options.m_oLimits.m_uInstructions = 1'000'000; // per Run() / Call()
//     options.m_oLimits.m_uHeapBytes = 64 * bloop::MB;
//     options.m_oModules.push_back(MakeGameModule());   // the script sees it as a global, next to std
//
//     bloop::Script script(options);
//     if (auto result = script.LoadFile("mods/my_mod/main.bloop"); !result)
//         Log(result.m_sError);                         // a compile error
//     else if (auto result = script.Run(); !result)
//         Log(result.m_sError);                         // a runtime error in the top level code
//
//     // every frame: an exported function of the mod
//     if (script.HasFunction("on_update"))
//         script.Call("on_update", { 0.016 });
//
// Errors in the script never throw into the host: they're returned in a Result, and the script can be
// called again afterwards. Scripts can't reach outside of the VM unless the host allows it
// (ScriptOptions::m_oEnvironment, which allows nothing by default), can't import files outside of the main
// file's directory by default, and the limits stop a script that runs forever or uses too much memory.
//
// Native modules: functions written in C++ that scripts can call. Bind<&Fn>() turns an ordinary C++ function
// into one (std/bind.hpp explains the parameter types it understands):
//
//     bloop::BloopInt Spawn(bloop::vm::VM& vm, bloop::standard::StringArg name, bloop::BloopDouble x) {
//         auto* game = vm.GetUserData<Game>();          // ScriptOptions::m_pUserData
//         return game->Spawn(std::string(name.View()), x);
//     }
//
//     bloop::standard::NativeDef MakeGameModule() {
//         using namespace bloop::standard;
//         return NativeDef("game", NativeObject{ .m_oFields = {
//             NativeField("spawn", Bind<&Spawn>()),
//             NativeField("version", MakeConstant(bloop::BloopInt{ 3 })),
//         }});
//     }
//
// and the script calls game.spawn("crate", 2.5);
//
// Events: AddEventFunctions(module) gives a module on(event, handler) and off(event, handler),
// and the host calls every handler of an event with Script::Emit:
//
//     game.on("player_died", (name) => { std.console.log(name, " died"); });   // in the script
//     script.Emit("player_died", { "bob" });                                   // in the host
//
// A native function can also keep any function it gets (a callback) with Function::Keep,
// and the host calls it later with Script::Call(function, args).

#include "utils/defs.hpp"
#include "std/environment.hpp"
#include "std/native.hpp"
#include "std/bind.hpp"
#include "vm/vm.hpp"
#include "vm/gc/handles.hpp" // RootScope, for native functions that allocate

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bloop {

	// a script value copied out of the VM (or into it): it doesn't depend on the VM or its garbage collector.
	// undefined, booleans, integers, doubles, strings, arrays and objects (keys in insertion order).
	// Values that can't be copied (functions) become an "opaque" value that only has their type name
	class HostValue {
	public:
		using Array = std::vector<HostValue>;
		using Object = std::vector<std::pair<std::string, HostValue>>;
		struct Opaque {
			std::string m_sType;
			[[nodiscard]] bool operator==(const Opaque&) const = default;
		};

		HostValue() = default; // undefined
		HostValue(bool value) : m_oData(value) {}
		HostValue(int value) : m_oData(static_cast<bloop::BloopInt>(value)) {}
		HostValue(bloop::BloopInt value) : m_oData(value) {}
		HostValue(double value) : m_oData(value) {}
		HostValue(const char* value) : m_oData(std::string(value)) {}
		HostValue(std::string value) : m_oData(std::move(value)) {}
		HostValue(std::string_view value) : m_oData(std::string(value)) {}
		HostValue(Array value) : m_oData(std::move(value)) {}
		HostValue(Object value) : m_oData(std::move(value)) {}
		HostValue(Opaque value) : m_oData(std::move(value)) {}

		[[nodiscard]] bool IsUndefined() const noexcept { return std::holds_alternative<std::monostate>(m_oData); }
		[[nodiscard]] bool IsBool() const noexcept { return std::holds_alternative<bool>(m_oData); }
		[[nodiscard]] bool IsInt() const noexcept { return std::holds_alternative<bloop::BloopInt>(m_oData); }
		[[nodiscard]] bool IsDouble() const noexcept { return std::holds_alternative<double>(m_oData); }
		[[nodiscard]] bool IsNumber() const noexcept { return IsInt() || IsDouble(); }
		[[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(m_oData); }
		[[nodiscard]] bool IsArray() const noexcept { return std::holds_alternative<Array>(m_oData); }
		[[nodiscard]] bool IsObject() const noexcept { return std::holds_alternative<Object>(m_oData); }
		[[nodiscard]] bool IsOpaque() const noexcept { return std::holds_alternative<Opaque>(m_oData); }

		// these throw std::bad_variant_access for the wrong type
		[[nodiscard]] bool AsBool() const { return std::get<bool>(m_oData); }
		[[nodiscard]] bloop::BloopInt AsInt() const { return std::get<bloop::BloopInt>(m_oData); }
		[[nodiscard]] double AsDouble() const { return std::get<double>(m_oData); }
		[[nodiscard]] double AsNumber() const { return IsInt() ? static_cast<double>(AsInt()) : AsDouble(); }
		[[nodiscard]] const std::string& AsString() const { return std::get<std::string>(m_oData); }
		[[nodiscard]] const Array& AsArray() const { return std::get<Array>(m_oData); }
		[[nodiscard]] const Object& AsObject() const { return std::get<Object>(m_oData); }
		[[nodiscard]] const Opaque& AsOpaque() const { return std::get<Opaque>(m_oData); }

		// a property of an object, or nullptr
		[[nodiscard]] const HostValue* Find(std::string_view key) const;

		// "undefined", "bool", "int", "double", "string", "array", "object" or the type of an opaque value
		[[nodiscard]] std::string TypeName() const;
		// readable text: a string as it is, everything else as json (opaque values as <type>)
		[[nodiscard]] std::string ToString() const;

		[[nodiscard]] bool operator==(const HostValue& other) const = default;

		std::variant<std::monostate, bool, bloop::BloopInt, double, std::string, Array, Object, Opaque> m_oData;
	};

	// a script function that the host keeps: an exported function (Script::GetFunction) or a callback
	// that a native function got (Function::Keep). It stays alive until it's destroyed, and becomes invalid
	// when the script is loaded again (or destroyed). Move-only
	class Function {
	public:
		Function() = default;

		// keeps a function value that a native function got as an argument (CallableArg::Get())
		[[nodiscard]] static Function Keep(bloop::vm::VM& vm, const bloop::vm::Value& callable);

		[[nodiscard]] bool IsValid() const noexcept { return m_oHandle.IsValid(); }
		explicit operator bool() const noexcept { return IsValid(); }

	private:
		friend class Script;
		bloop::vm::Persistent m_oHandle;
		const bloop::vm::VM* m_pVM{};
	};

	// adds on(event, handler) and off(event, handler) to a host module, for Script::Emit.
	// on: the handler gets called with the event's arguments (adding the same handler twice does nothing).
	// off: removes it, returns whether it was there
	void AddEventFunctions(bloop::standard::NativeDef& module);

	struct ScriptOptions {
		// what the standard library may do (no files, no process by default) and where std.console.log writes
		bloop::standard::Environment m_oEnvironment;

		// instruction budget and heap limit (no limits by default)
		bloop::vm::Limits m_oLimits;

		// the host's native modules: each one is a global that every file of the script can use
		std::vector<bloop::standard::NativeDef> m_oModules;

		// imports can only load files inside this directory. Empty: the main file's directory
		// (or LoadSource's directory). m_bUnrestrictedImports turns the check off
		std::filesystem::path m_oImportRoot;
		bool m_bUnrestrictedImports{};

		// VM::GetUserData() in native functions
		void* m_pUserData{};

		// a global with on() and off() (see AddEventFunctions), for example "events". Empty: none
		std::string m_sEventsModule;

		// also print runtime errors to std::cout (they're always in Result::m_sError)
		bool m_bPrintErrors{};
	};

	struct Result {
		enum class Status { ok, exited, compile_error, runtime_error, usage_error };

		// ok: the script did what was asked. exited: it called std.process.exit (not an error, but it can't
		// be called anymore). usage_error: the host asked for something that doesn't exist (or in the wrong order)
		Status m_eStatus{ Status::ok };
		HostValue m_oValue;         // the return value
		std::string m_sError;       // the message (and the stack trace of runtime errors)
		bloop::BloopInt m_iExitCode{};

		[[nodiscard]] bool Ok() const noexcept { return m_eStatus == Status::ok || m_eStatus == Status::exited; }

		[[nodiscard]] static Result Success(HostValue value) {
			Result r;
			r.m_oValue = std::move(value);
			return r;
		}
		[[nodiscard]] static Result Failure(Status status, std::string error) {
			Result r;
			r.m_eStatus = status;
			r.m_sError = std::move(error);
			return r;
		}
		[[nodiscard]] static Result Exited(bloop::BloopInt code) {
			Result r;
			r.m_eStatus = Status::exited;
			r.m_iExitCode = code;
			return r;
		}
		explicit operator bool() const noexcept { return Ok(); }
	};

	// a script (a main file and the modules it imports) and the VM that runs it
	class Script {
	public:
		explicit Script(ScriptOptions options = {});
		~Script();

		Script(Script&&) noexcept;
		Script& operator=(Script&&) noexcept;
		Script(const Script&) = delete;
		Script& operator=(const Script&) = delete;

		// compiles a file and everything it imports (replacing what was loaded before)
		[[nodiscard]] Result LoadFile(const std::filesystem::path& path);
		// the same for code that isn't in a file. Its imports are relative to `directory` (empty: the working directory)
		[[nodiscard]] Result LoadSource(std::string_view source, const std::filesystem::path& directory = {}, std::string_view name = "main");

		// runs the top level code once (the imported modules first). The exports exist after this,
		// and the value is what the top level code returned
		[[nodiscard]] Result Run();

		// calls a function that the main file exports (after Run). The arguments are copied into the script
		[[nodiscard]] Result Call(std::string_view function, std::span<const HostValue> args);
		[[nodiscard]] Result Call(std::string_view function, std::initializer_list<HostValue> args = {});

		// calls a function that the host kept (after Run)
		[[nodiscard]] Result Call(const Function& function, std::span<const HostValue> args);
		[[nodiscard]] Result Call(const Function& function, std::initializer_list<HostValue> args = {});

		// an exported function to keep (after Run), or an invalid Function
		[[nodiscard]] Function GetFunction(std::string_view name);

		// calls every handler of the event (in the order they were added) with the same arguments.
		// A handler that fails doesn't stop the others: the result is the first error. The value is
		// how many handlers were called. Each handler gets its own instruction budget
		[[nodiscard]] Result Emit(std::string_view event, std::span<const HostValue> args);
		[[nodiscard]] Result Emit(std::string_view event, std::initializer_list<HostValue> args = {});
		[[nodiscard]] bool HasHandlers(std::string_view event) const;

		// the value of an export (after Run)
		[[nodiscard]] Result Get(std::string_view name) const;

		// whether the main file exports a function with this name (after Run)
		[[nodiscard]] bool HasFunction(std::string_view name) const;
		// the names of the main file's exports
		[[nodiscard]] std::vector<std::string> Exports() const;

		[[nodiscard]] bool IsLoaded() const noexcept;
		[[nodiscard]] bool HasRun() const noexcept;
		[[nodiscard]] bool HasExited() const noexcept;

		[[nodiscard]] const ScriptOptions& Options() const noexcept;

		// the VM itself, for anything this class doesn't cover (nullptr before LoadFile/LoadSource)
		[[nodiscard]] bloop::vm::VM* GetVM() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;
	};

	// copies a value out of the VM (doesn't allocate in the VM)
	[[nodiscard]] HostValue ToHostValue(const bloop::vm::Value& value);
	// copies a value into the VM. The result is only valid until the next allocation (keep it in a RootScope)
	[[nodiscard]] bloop::vm::Value FromHostValue(bloop::vm::VM& vm, const HostValue& value);
}
