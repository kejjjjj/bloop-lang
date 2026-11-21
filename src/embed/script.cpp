#include "bloop.hpp"
#include "module/module.hpp"
#include "ast/ast.hpp"
#include "metadata/metadata.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/heap/heap.hpp"
#include "vm/gc/handles.hpp"
#include "vm/tasks.hpp"
#include "std/lib/task/stdtask.hpp"
#include "utils/exception.hpp"
#include "utils/fmt.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include "std/lib/common.hpp"

using namespace bloop;
using bloop::vm::Value;
using bloop::vm::Object;

/***********************************************************************
 *                              HostValue
 ***********************************************************************/

const HostValue* HostValue::Find(std::string_view key) const {
	if (!IsObject())
		return nullptr;

	for (const auto& [k, v] : AsObject()) {
		if (k == key)
			return &v;
	}
	return nullptr;
}

std::string HostValue::TypeName() const {
	switch (m_oData.index()) {
	case 0: return "undefined";
	case 1: return "bool";
	case 2: return "int";
	case 3: return "double";
	case 4: return "string";
	case 5: return "array";
	case 6: return "object";
	default: return IsHandle() ? AsHandle().m_sType : AsOpaque().m_sType;
	}
}

namespace {
	void Quote(std::string& out, const std::string& str) {
		out += '"';
		for (const auto c : str) {
			switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default: out += c;
			}
		}
		out += '"';
	}

	void Write(std::string& out, const HostValue& value, bool topLevel) {
		if (value.IsString()) {
			if (topLevel)
				out += value.AsString();
			else
				Quote(out, value.AsString());
		} else if (value.IsUndefined()) {
			out += "undefined";
		} else if (value.IsBool()) {
			out += value.AsBool() ? "true" : "false";
		} else if (value.IsInt()) {
			out += std::to_string(value.AsInt());
		} else if (value.IsDouble()) {
			std::ostringstream stream;
			stream.imbue(std::locale::classic());
			stream << std::setprecision(17) << value.AsDouble();
			out += stream.str();
		} else if (value.IsArray()) {
			out += '[';
			for (std::size_t i = 0; i < value.AsArray().size(); i++) {
				if (i)
					out += ", ";
				Write(out, value.AsArray()[i], false);
			}
			out += ']';
		} else if (value.IsObject()) {
			out += '{';
			for (std::size_t i = 0; i < value.AsObject().size(); i++) {
				if (i)
					out += ", ";
				Quote(out, value.AsObject()[i].first);
				out += ": ";
				Write(out, value.AsObject()[i].second, false);
			}
			out += '}';
		} else if (value.IsHandle()) {
			out += value.AsHandle().m_sType + '#' + std::to_string(value.AsHandle().m_uId);
		} else {
			out += '<' + value.AsOpaque().m_sType + '>';
		}
	}
}

std::string HostValue::ToString() const {
	std::string out;
	Write(out, *this, true);
	return out;
}

/***********************************************************************
 *                              conversions
 ***********************************************************************/

namespace {
	constexpr std::size_t MAX_CONVERSION_DEPTH = 512u;

	HostValue ToHost(const Value& value, std::vector<const Object*>& path) {
		switch (value.type) {
		case Value::Type::t_undefined: return {};
		case Value::Type::t_bool:      return HostValue(static_cast<bool>(value.b));
		case Value::Type::t_int:       return HostValue(value.i);
		case Value::Type::t_uint:      return HostValue(static_cast<bloop::BloopInt>(value.u));
		case Value::Type::t_double:    return HostValue(static_cast<double>(value.d));
		case Value::Type::t_object:    break;
		}

		const auto* obj = value.obj;
		if (obj->IsString())
			return HostValue(std::string(obj->string.data, static_cast<std::size_t>(obj->string.len)));

		if (obj->IsHandle())
			return HostValue(HostValue::Handle{ *obj->handle.type, obj->handle.id });

		if (!obj->IsArray() && !obj->IsObject())
			return HostValue(HostValue::Opaque{ value.TypeToString() });

		// a value that contains itself (or is absurdly deep) can't be copied
		if (std::ranges::find(path, obj) != path.end() || path.size() >= MAX_CONVERSION_DEPTH)
			return HostValue(HostValue::Opaque{ BLOOPTEXT("cycle") });

		path.push_back(obj);
		HostValue result;

		if (obj->IsArray()) {
			HostValue::Array array;
			array.reserve(static_cast<std::size_t>(obj->array.count));
			for (bloop::BloopInt i = 0; i < obj->array.count; i++)
				array.push_back(ToHost(obj->array.values[i], path));
			result = HostValue(std::move(array));
		} else {
			HostValue::Object object;
			for (const auto slot : obj->ObjectSlotsInOrder()) {
				const auto& entry = obj->object.entries[slot];
				object.emplace_back(std::string(entry.key.obj->string.data, static_cast<std::size_t>(entry.key.obj->string.len)),
					ToHost(entry.value, path));
			}
			result = HostValue(std::move(object));
		}

		path.pop_back();
		return result;
	}

	Value FromHost(bloop::vm::VM& vm, const HostValue& value, std::size_t depth) {
		if (depth >= MAX_CONVERSION_DEPTH)
			throw std::invalid_argument("the value is nested too deep");

		auto& heap = vm.GetHeap();
		auto& gc = vm.GetGC();

		if (value.IsUndefined()) return Value{};
		if (value.IsBool())      return Value{ static_cast<bloop::BloopBool>(value.AsBool()) };
		if (value.IsInt())       return Value{ value.AsInt() };
		if (value.IsDouble())    return Value{ static_cast<bloop::BloopDouble>(value.AsDouble()) };
		if (value.IsString())    return Value{ heap.AllocString(value.AsString().data(), value.AsString().size()) };
		if (value.IsHandle())    return Value{ heap.AllocHandle(vm.GetHandleType(value.AsHandle().m_sType), value.AsHandle().m_uId) };

		if (value.IsArray()) {
			const auto& items = value.AsArray();
			bloop::vm::RootScope scope(gc);
			const auto array = scope.Keep(heap.AllocArray(items.size()));

			for (std::size_t i = 0; i < items.size(); i++) {
				// the newest allocation: nothing moves it before it's stored
				const auto item = FromHost(vm, items[i], depth + 1u);
				array->array.values[i] = item;
				gc.WriteBarrier(array.Obj(), item);
			}
			return array.Get();
		}

		if (value.IsObject()) {
			const auto& members = value.AsObject();
			bloop::vm::RootScope scope(gc);
			const auto object = scope.Keep(heap.AllocObject(0u));

			for (const auto& [key, member] : members) {
				bloop::vm::RootScope memberScope(gc);
				const auto item = memberScope.Keep(FromHost(vm, member, depth + 1u));
				auto* keyString = heap.AllocString(key.data(), key.size());
				heap.ObjectSet(object.Obj(), Value{ keyString }, item.Get());
			}
			return object.Get();
		}

		throw std::invalid_argument("a value of type \"" + value.TypeName() + "\" can't be passed to a script");
	}
}

HostValue bloop::ToHostValue(const Value& value) {
	std::vector<const Object*> path;
	return ToHost(value, path);
}

Value bloop::FromHostValue(bloop::vm::VM& vm, const HostValue& value) {
	return FromHost(vm, value, 0u);
}

/***********************************************************************
 *                              events
 ***********************************************************************/

namespace {
	struct EventRegistry {
		struct Handler {
			bloop::BloopUInt m_uId{};
			bloop::vm::Persistent m_oFunction;
		};

		std::unordered_map<std::string, std::vector<Handler>> m_oEvents;
		bloop::BloopUInt m_uNextId{};

		[[nodiscard]] static EventRegistry& From(bloop::vm::VM& vm) {
			auto* registry = static_cast<EventRegistry*>(vm.GetEmbedding());
			if (!registry)
				bloop::standard::lib::Fail(BLOOPTEXT("events are only available in scripts that a bloop::Script runs"));
			return *registry;
		}
	};

	void On(bloop::vm::VM& vm, bloop::standard::StringArg event, bloop::standard::CallableArg handler) {
		auto& handlers = EventRegistry::From(vm).m_oEvents[event.Str()];

		if (std::ranges::any_of(handlers, [&](const auto& h) { return h.m_oFunction.Holds(handler.Get()); }))
			return; // already there

		auto& registry = EventRegistry::From(vm);
		handlers.push_back({ registry.m_uNextId++, vm.GetGC().MakePersistent(handler.Get()) });
	}

	bloop::BloopBool Off(bloop::vm::VM& vm, bloop::standard::StringArg event, bloop::standard::CallableArg handler) {
		auto& events = EventRegistry::From(vm).m_oEvents;
		const auto itr = events.find(event.Str());
		if (itr == events.end())
			return false;

		auto& handlers = itr->second;
		const auto handlerItr = std::ranges::find_if(handlers, [&](const auto& h) { return h.m_oFunction.Holds(handler.Get()); });
		if (handlerItr == handlers.end())
			return false;

		handlers.erase(handlerItr);
		if (handlers.empty())
			events.erase(itr);
		return true;
	}
}

void bloop::AddEventFunctions(bloop::standard::NativeDef& module) {
	using namespace bloop::standard;
	auto* object = std::get_if<std::shared_ptr<NativeObject>>(&module.m_oData);
	if (!object || !*object)
		throw std::invalid_argument("AddEventFunctions: the module has to be an object");

	(*object)->m_oFields.emplace_back(BLOOPTEXT("on"), Bind<&On>());
	(*object)->m_oFields.emplace_back(BLOOPTEXT("off"), Bind<&Off>());
}

Function Function::Keep(bloop::vm::VM& vm, const Value& callable) {
	if (!callable.IsCallable())
		throw std::invalid_argument("Function::Keep: the value isn't a function");

	Function result;
	result.m_oHandle = vm.GetGC().MakePersistent(callable);
	result.m_pVM = &vm;
	return result;
}

/***********************************************************************
 *                              Script
 ***********************************************************************/

namespace {
	// a position (lines counted across every file of the program) in its own file
	ErrorLocation Locate(const bloop::metadata::Metadata& metadata, const bloop::BloopString& message, const bloop::CodePosition& position) {
		const auto [line, column] = position;

		ErrorLocation location;
		location.m_sMessage = message;
		location.m_uLine = line;
		location.m_uColumn = column;

		if (const auto* file = metadata.FindSourceFile(line)) {
			location.m_sFile = file->m_sName;
			location.m_uLine = line - file->m_uFirstLine + 1u;
		}
		if (line >= 1u && line - 1u < metadata.m_oLineMap.size()) {
			location.m_sSourceLine = std::string(metadata.m_oLineMap[line - 1u]);
			// (a file with windows line endings: the line without its \r)
			while (!location.m_sSourceLine.empty() && (location.m_sSourceLine.back() == '\r' || location.m_sSourceLine.back() == '\n'))
				location.m_sSourceLine.pop_back();
		}
		return location;
	}
}

struct Script::Impl {
	ScriptOptions m_oOptions;

	// the VM references the metadata, and the metadata references the source code it owns
	std::unique_ptr<bloop::metadata::Metadata> m_pMetadata;
	std::unique_ptr<bloop::ast::Program> m_pProgram;
	std::unique_ptr<bloop::vm::VM> m_pVM;
	EventRegistry m_oEvents; // (after the VM: destroyed before it)
	bool m_bHasRun{};

	[[nodiscard]] Result Load(const std::function<std::unique_ptr<bloop::ast::Program>(bloop::metadata::Metadata&, const bloop::module::CompileOptions&)>& compile,
		const std::filesystem::path& defaultImportRoot) {

		m_oEvents.m_oEvents.clear();
		m_pVM.reset();
		m_pProgram.reset();
		m_pMetadata = std::make_unique<bloop::metadata::Metadata>();
		m_bHasRun = false;

		bloop::module::CompileOptions compileOptions;
		compileOptions.m_oHostModules = m_oOptions.m_oModules;
		compileOptions.m_fnReadFile = m_oOptions.m_fnReadFile;
		if (!m_oOptions.m_sEventsModule.empty()) {
			bloop::standard::NativeDef events(m_oOptions.m_sEventsModule, bloop::standard::NativeObject{});
			AddEventFunctions(events);
			compileOptions.m_oHostModules.push_back(std::move(events));
		}
		if (!m_oOptions.m_bUnrestrictedImports)
			compileOptions.m_oImportRoot = m_oOptions.m_oImportRoot.empty() ? defaultImportRoot : m_oOptions.m_oImportRoot;

		try {
			m_pProgram = compile(*m_pMetadata, compileOptions);
		} catch (bloop::exception::CompileError& ex) {
			auto result = Result::Failure(Result::Status::compile_error, ex.what());
			if (ex.m_oPosition)
				result.m_oLocation = Locate(*m_pMetadata, ex.m_sMessage, *ex.m_oPosition);
			m_pMetadata.reset();
			return result;
		} catch (std::exception& ex) {
			m_pMetadata.reset();
			return Result::Failure(Result::Status::compile_error, ex.what());
		}

		m_pVM = std::make_unique<bloop::vm::VM>(*m_pMetadata, m_oOptions.m_oEnvironment);
		m_pVM->SetLimits(m_oOptions.m_oLimits);
		m_pVM->SetUserData(m_oOptions.m_pUserData);
		m_pVM->SetPrintErrors(m_oOptions.m_bPrintErrors);
		m_pVM->SetEmbedding(&m_oEvents);
		return {};
	}

	[[nodiscard]] Result Ended(std::optional<Value> value) const {
		if (const auto code = m_pVM->GetExitCode())
			return Result::Exited(*code);

		if (!value || m_pVM->Failed()) {
			auto result = Result::Failure(Result::Status::runtime_error, m_pVM->GetError());
			for (const auto& call : m_pVM->GetErrorTrace())
				result.m_oTrace.push_back(Locate(*m_pMetadata, result.m_oTrace.empty() ? m_pVM->GetErrorMessage() : bloop::BloopString{}, call.m_oPosition));
			if (!result.m_oTrace.empty())
				result.m_oLocation = result.m_oTrace.front();
			return result;
		}

		return Result::Success(ToHostValue(*value));
	}

	// the problem with calling into the script right now, if there is one
	[[nodiscard]] std::optional<Result> CheckReady(bool needsRun) const {
		const auto Usage = [](std::string message) { return Result::Failure(Result::Status::usage_error, std::move(message)); };

		if (!m_pVM)
			return Usage("no script is loaded");
		if (m_pVM->GetExitCode())
			return Usage("the script has exited");
		if (m_pVM->IsRunning())
			return Usage("the script is already running (a native function can call script functions with VM::Call)");
		if (needsRun && !m_bHasRun)
			return Usage("the script hasn't run yet (call Run() first)");
		if (!needsRun && m_bHasRun)
			return Usage("the script has already run");
		return std::nullopt;
	}

	// converts the arguments, then calls the function that getCallee returns (read after the conversions,
	// which can move things). An empty callee means it disappeared in the meantime
	template<typename GetCallee>
	[[nodiscard]] Result Invoke(GetCallee&& getCallee, std::span<const HostValue> args) {
		auto& vm = *m_pVM;
		std::optional<Value> result;

		// the converted arguments stay rooted until they're on the VM's stack
		bloop::vm::RootScope scope(vm.GetGC());
		std::vector<bloop::vm::Local> converted;
		converted.reserve(args.size());

		try {
			for (const auto& arg : args)
				converted.push_back(scope.Keep(FromHostValue(vm, arg)));
		} catch (std::invalid_argument& ex) {
			return Result::Failure(Result::Status::usage_error, ex.what());
		} catch (std::exception& ex) {
			return Result::Failure(Result::Status::runtime_error, ex.what()); // out of memory
		}

		std::vector<Value> values;
		values.reserve(converted.size());
		for (const auto& local : converted)
			values.push_back(local.Get());

		const Value callee = getCallee();
		result = vm.CallFromHost(callee, values);
		return Ended(result);
	}

	[[nodiscard]] const Value* FindExport(std::string_view name) const {
		if (!m_pVM)
			return nullptr;
		const auto& exports = m_pMetadata->m_oExports;
		const auto itr = exports.find(bloop::BloopString(name));
		return itr == exports.end() ? nullptr : &m_pVM->GetGlobal(itr->second);
	}
};

Script::Script(ScriptOptions options) : m_pImpl(std::make_unique<Impl>()) {
	m_pImpl->m_oOptions = std::move(options);
}
Script::~Script() = default;
Script::Script(Script&&) noexcept = default;
Script& Script::operator=(Script&&) noexcept = default;

Result Script::LoadFile(const std::filesystem::path& path) {
	std::error_code ec;
	const auto directory = std::filesystem::absolute(path, ec).parent_path();

	return m_pImpl->Load([&](bloop::metadata::Metadata& metadata, const bloop::module::CompileOptions& options) {
		return bloop::module::CompileFile(path, metadata, options);
	}, directory);
}

Result Script::LoadSource(std::string_view source, const std::filesystem::path& directory, std::string_view name) {
	const auto dir = directory.empty() ? std::filesystem::current_path() : directory;

	return m_pImpl->Load([&](bloop::metadata::Metadata& metadata, const bloop::module::CompileOptions& options) {
		return bloop::module::CompileSource(source, metadata, dir, bloop::BloopString(name), options);
	}, dir);
}

Result Script::Run() {
	if (auto problem = m_pImpl->CheckReady(false))
		return *problem;

	m_pImpl->m_bHasRun = true;
	const auto value = m_pImpl->m_pVM->Run();
	return m_pImpl->Ended(value);
}

Result Script::Call(std::string_view function, std::initializer_list<HostValue> args) {
	return Call(function, std::span<const HostValue>(args.begin(), args.size()));
}

Result Script::Call(std::string_view function, std::span<const HostValue> args) {
	if (auto problem = m_pImpl->CheckReady(true))
		return *problem;

	const auto* callee = m_pImpl->FindExport(function);
	if (!callee)
		return Result::Failure(Result::Status::usage_error, bloop::fmt::format(BLOOPTEXT("the script doesn't export \"{}\""), bloop::BloopString(function)));
	if (!callee->IsCallable())
		return Result::Failure(Result::Status::usage_error, bloop::fmt::format(BLOOPTEXT("\"{}\" isn't a function, it's {}"), bloop::BloopString(function), callee->TypeToString()));

	return m_pImpl->Invoke([callee] { return *callee; }, args); // (a global: a root)
}

Result Script::Call(const Function& function, std::initializer_list<HostValue> args) {
	return Call(function, std::span<const HostValue>(args.begin(), args.size()));
}

Result Script::Call(const Function& function, std::span<const HostValue> args) {
	if (auto problem = m_pImpl->CheckReady(true))
		return *problem;

	if (!function.IsValid() || function.m_pVM != m_pImpl->m_pVM.get())
		return Result::Failure(Result::Status::usage_error, "the function isn't valid (it belongs to another script, or to one that was loaded again)");

	return m_pImpl->Invoke([&function] { return function.m_oHandle.Get(); }, args);
}

Function Script::GetFunction(std::string_view name) {
	const auto* value = m_pImpl->m_bHasRun ? m_pImpl->FindExport(name) : nullptr;
	if (!value || !value->IsCallable())
		return {};
	return Function::Keep(*m_pImpl->m_pVM, *value);
}

Result Script::Emit(std::string_view event, std::initializer_list<HostValue> args) {
	return Emit(event, std::span<const HostValue>(args.begin(), args.size()));
}

Result Script::Emit(std::string_view event, std::span<const HostValue> args) {
	if (auto problem = m_pImpl->CheckReady(true))
		return *problem;

	auto& events = m_pImpl->m_oEvents.m_oEvents;
	const auto name = std::string(event);

	// handlers can add and remove handlers: go through the ones that were there at the start,
	// skipping those that get removed before their turn
	std::vector<bloop::BloopUInt> ids;
	if (const auto itr = events.find(name); itr != events.end()) {
		for (const auto& handler : itr->second)
			ids.push_back(handler.m_uId);
	}

	auto& vm = *m_pImpl->m_pVM;

	// the arguments are converted once: every handler gets the same values (like js)
	bloop::vm::RootScope scope(vm.GetGC());
	std::vector<bloop::vm::Local> converted;
	converted.reserve(args.size());
	try {
		for (const auto& arg : args)
			converted.push_back(scope.Keep(FromHostValue(vm, arg)));
	} catch (std::invalid_argument& ex) {
		return Result::Failure(Result::Status::usage_error, ex.what());
	} catch (std::exception& ex) {
		return Result::Failure(Result::Status::runtime_error, ex.what());
	}

	std::optional<Result> firstError;
	bloop::BloopInt called{};

	for (const auto id : ids) {
		const auto itr = events.find(name);
		if (itr == events.end())
			break;

		const auto handler = std::ranges::find_if(itr->second, [id](const auto& h) { return h.m_uId == id; });
		if (handler == itr->second.end())
			continue; // removed by an earlier handler

		std::vector<Value> values;
		values.reserve(converted.size());
		for (const auto& local : converted)
			values.push_back(local.Get());

		const Value callee = handler->m_oFunction.Get();
		const auto result = m_pImpl->Ended(vm.CallFromHost(callee, values));
		called++;

		if (result.m_eStatus == Result::Status::exited)
			return result;
		if (!result.Ok() && !firstError)
			firstError = result;
	}

	if (firstError)
		return *firstError;
	return Result::Success(HostValue(called));
}

bool Script::HasHandlers(std::string_view event) const {
	const auto& events = m_pImpl->m_oEvents.m_oEvents;
	const auto itr = events.find(std::string(event));
	return itr != events.end() && !itr->second.empty();
}

Result Script::RunTasks() {
	if (auto problem = m_pImpl->CheckReady(true))
		return *problem;

	std::optional<Result> firstError;
	std::optional<Result> exited;

	const auto ran = bloop::standard::RunDueTasks(*m_pImpl->m_pVM, [&](const std::optional<Value>& value) {
		auto result = m_pImpl->Ended(value);
		if (result.m_eStatus == Result::Status::exited) {
			exited = std::move(result);
			return false;
		}
		if (!result.Ok() && !firstError)
			firstError = std::move(result);
		return true;
	});

	if (exited)
		return *exited;
	if (firstError)
		return *firstError;
	return Result::Success(HostValue(static_cast<bloop::BloopInt>(ran)));
}

std::size_t Script::TaskCount() const noexcept {
	return m_pImpl->m_pVM ? m_pImpl->m_pVM->GetTasks().m_oTasks.size() : 0u;
}

std::optional<double> Script::NextTaskTime() const {
	return m_pImpl->m_pVM ? bloop::standard::NextTaskTime(*m_pImpl->m_pVM) : std::nullopt;
}

Result Script::Get(std::string_view name) const {
	if (!m_pImpl->m_pVM || !m_pImpl->m_bHasRun)
		return Result::Failure(Result::Status::usage_error, "the script hasn't run yet (call Run() first)");

	const auto* value = m_pImpl->FindExport(name);
	if (!value)
		return Result::Failure(Result::Status::usage_error, bloop::fmt::format(BLOOPTEXT("the script doesn't export \"{}\""), bloop::BloopString(name)));

	return Result::Success(ToHostValue(*value));
}

bool Script::HasFunction(std::string_view name) const {
	const auto* value = m_pImpl->m_bHasRun ? m_pImpl->FindExport(name) : nullptr;
	return value && value->IsCallable();
}

std::vector<std::string> Script::Exports() const {
	std::vector<std::string> names;
	if (m_pImpl->m_pMetadata) {
		for (const auto& [name, slot] : m_pImpl->m_pMetadata->m_oExports)
			names.push_back(name);
	}
	std::ranges::sort(names);
	return names;
}

bool Script::IsLoaded() const noexcept { return m_pImpl->m_pVM != nullptr; }
bool Script::HasRun() const noexcept { return m_pImpl->m_bHasRun; }
bool Script::HasExited() const noexcept { return m_pImpl->m_pVM && m_pImpl->m_pVM->GetExitCode().has_value(); }
const ScriptOptions& Script::Options() const noexcept { return m_pImpl->m_oOptions; }
bloop::vm::VM* Script::GetVM() noexcept { return m_pImpl->m_pVM.get(); }
