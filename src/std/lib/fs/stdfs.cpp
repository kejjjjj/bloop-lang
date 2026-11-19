#include "std/lib/fs/stdfs.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"
#include "utils/filesystem.hpp"
#include "utils/fmt.hpp"

#include <algorithm>
#include <ranges>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// std.fs: files and directories.
// failures that a script can't prevent (missing files, permissions...) return undefined or false,
// using a path outside of the environment's file system root terminates the script

namespace {
	namespace fs = std::filesystem;

	// strings are utf-8, but std::filesystem::path(std::string) uses the ANSI code page on windows
	[[nodiscard]] fs::path ToPath(bloop::BloopStringView str) {
		return fs::path(std::u8string(reinterpret_cast<const char8_t*>(str.data()), str.size()));
	}
	[[nodiscard]] bloop::BloopString FromPath(const fs::path& path) {
		const auto str = path.u8string();
		return bloop::BloopString(reinterpret_cast<const char*>(str.data()), str.size());
	}

	// the file that a script means: relative to the root if the environment has one, and never outside of it
	[[nodiscard]] fs::path Resolve(VM& vm, const char* function, StringArg path) {
		const auto str = path.View();

		if (str.empty())
			Fail(bloop::fmt::format(BLOOPTEXT("std.fs.{}: the path is empty"), function));
		if (str.find('\0') != bloop::BloopStringView::npos)
			Fail(bloop::fmt::format(BLOOPTEXT("std.fs.{}: the path contains a null character"), function));

		const auto& root = vm.GetEnvironment().m_oFileSystemRoot;
		if (root.empty())
			return ToPath(str);

		const auto normalizedRoot = bloop::filesystem::Normalize(root);
		auto result = bloop::filesystem::Normalize(normalizedRoot / ToPath(str));

		if (!bloop::filesystem::IsInside(result, normalizedRoot))
			Fail(bloop::fmt::format(BLOOPTEXT("std.fs.{}: \"{}\" is outside of the directory that scripts can access"), function, bloop::BloopString(str)));

		return result;
	}

	// the contents of a file, or undefined if it can't be read
	std::optional<bloop::BloopString> Read(VM& vm, StringArg path) {
		const auto file = Resolve(vm, "read", path);

		std::error_code ec;
		if (!fs::is_regular_file(file, ec))
			return std::nullopt;

		std::ifstream stream(file, std::ios::binary);
		if (!stream)
			return std::nullopt;

		auto contents = bloop::BloopString(std::istreambuf_iterator<bloop::BloopChar>(stream), std::istreambuf_iterator<bloop::BloopChar>());
		if (stream.bad())
			return std::nullopt;

		return contents;
	}

	bloop::BloopBool WriteFile(VM& vm, const char* function, StringArg path, StringArg text, std::ios::openmode mode) {
		const auto file = Resolve(vm, function, path);

		std::ofstream stream(file, std::ios::binary | mode);
		if (!stream)
			return false;

		const auto data = text.View();
		stream.write(data.data(), static_cast<std::streamsize>(data.size()));
		stream.close();
		return !stream.fail();
	}

	// replaces the file (creates it if needed), returns whether it worked
	bloop::BloopBool Write(VM& vm, StringArg path, StringArg text) {
		return WriteFile(vm, "write", path, text, std::ios::trunc);
	}
	// adds to the end of the file (creates it if needed), returns whether it worked
	bloop::BloopBool Append(VM& vm, StringArg path, StringArg text) {
		return WriteFile(vm, "append", path, text, std::ios::app);
	}

	bloop::BloopBool Exists(VM& vm, StringArg path) {
		std::error_code ec;
		return fs::exists(Resolve(vm, "exists", path), ec);
	}
	bloop::BloopBool IsFile(VM& vm, StringArg path) {
		std::error_code ec;
		return fs::is_regular_file(Resolve(vm, "is_file", path), ec);
	}
	bloop::BloopBool IsDirectory(VM& vm, StringArg path) {
		std::error_code ec;
		return fs::is_directory(Resolve(vm, "is_dir", path), ec);
	}

	// the names of the files and directories in a directory (sorted), or undefined if it can't be listed
	Value List(VM& vm, StringArg path) {
		const auto directory = Resolve(vm, "list", path);

		std::error_code ec;
		auto itr = fs::directory_iterator(directory, ec);
		if (ec)
			return Value{};

		std::vector<bloop::BloopString> names;
		for (const auto end = fs::directory_iterator(); itr != end; itr.increment(ec)) {
			if (ec)
				return Value{};
			names.push_back(FromPath(itr->path().filename()));
		}
		std::ranges::sort(names);

		if (static_cast<bloop::BloopInt>(names.size()) > MAX_ARRAY_LENGTH)
			Fail(BLOOPTEXT("std.fs.list: too many files"));

		auto& heap = vm.GetHeap();
		RootScope scope(vm.GetGC());
		const auto result = scope.Keep(heap.AllocArray(names.size()));

		for (std::size_t i = 0; i < names.size(); i++) {
			auto* name = heap.AllocString(names[i].data(), names[i].size());
			result->array.values[i] = Value{ name };
			vm.GetGC().WriteBarrier(result.Obj(), name);
		}
		return result.Get();
	}

	// creates the directory and any missing parents, returns whether the directory exists now
	bloop::BloopBool MakeDirectory(VM& vm, StringArg path) {
		const auto directory = Resolve(vm, "make_dir", path);
		std::error_code ec;
		fs::create_directories(directory, ec);
		return fs::is_directory(directory, ec);
	}

	// removes a file or an empty directory (never anything recursively), returns whether something was removed
	bloop::BloopBool Remove(VM& vm, StringArg path) {
		std::error_code ec;
		return fs::remove(Resolve(vm, "remove", path), ec);
	}

	// moves or renames a file or a directory, returns whether it worked
	bloop::BloopBool Rename(VM& vm, StringArg from, StringArg to) {
		const auto source = Resolve(vm, "rename", from);
		const auto destination = Resolve(vm, "rename", to);
		std::error_code ec;
		fs::rename(source, destination, ec);
		return !ec;
	}
}

NativeDef bloop::standard::GetFileSystemDefinitions() {
	return NativeDef(BLOOPTEXT("fs"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("read"), Bind<&Read>()),
			NativeField(BLOOPTEXT("write"), Bind<&Write>()),
			NativeField(BLOOPTEXT("append"), Bind<&Append>()),
			NativeField(BLOOPTEXT("exists"), Bind<&Exists>()),
			NativeField(BLOOPTEXT("is_file"), Bind<&IsFile>()),
			NativeField(BLOOPTEXT("is_dir"), Bind<&IsDirectory>()),
			NativeField(BLOOPTEXT("list"), Bind<&List>()),
			NativeField(BLOOPTEXT("make_dir"), Bind<&MakeDirectory>()),
			NativeField(BLOOPTEXT("remove"), Bind<&Remove>()),
			NativeField(BLOOPTEXT("rename"), Bind<&Rename>()),
		}
	});
}
