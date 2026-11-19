#include "tests/vm/std/helpers.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

using namespace bloop::test::std_lib;
namespace fs = std::filesystem;

namespace {
	// a new empty directory for one test (removed afterwards)
	class TempDirectory {
	public:
		TempDirectory() {
			std::random_device rd;
			m_oPath = fs::temp_directory_path() / ("bloop_fs_test_" + std::to_string(rd()) + std::to_string(rd()));
			fs::create_directories(m_oPath / "root");
		}
		~TempDirectory() {
			std::error_code ec;
			fs::remove_all(m_oPath, ec);
		}
		TempDirectory(const TempDirectory&) = delete;
		TempDirectory& operator=(const TempDirectory&) = delete;

		// scripts can only reach this one
		[[nodiscard]] fs::path Root() const { return m_oPath / "root"; }
		// next to the root, so outside of it
		[[nodiscard]] fs::path Outside() const { return m_oPath; }

		[[nodiscard]] bloop::standard::Environment Environment() const {
			auto environment = bloop::standard::Environment::Unrestricted();
			environment.m_oFileSystemRoot = Root();
			return environment;
		}

	private:
		fs::path m_oPath;
	};

	struct Result {
		std::optional<Value> value;
		std::string output;
	};
	Result Run(const TempDirectory& dir, const std::string& src) {
		Result result;
		result.value = bloop::test::TEST_ExecuteBuffer(src, dir.Environment());
		result.output = bloop::test::TEST_LastOutput();
		return result;
	}
	std::string RunToString(const TempDirectory& dir, const std::string& src) {
		const auto result = Run(dir, src);
		INFO("script: " << src);
		INFO("output: " << result.output);
		REQUIRE(result.value.has_value());
		return result.value->ValueToString();
	}
	void RequireError(const TempDirectory& dir, const std::string& src, const std::string& message) {
		const auto result = Run(dir, src);
		INFO("script: " << src);
		INFO("output: " << result.output);
		REQUIRE(result.output.find("runtime error") != std::string::npos);
		REQUIRE(result.output.find(message) != std::string::npos);
	}
	std::string ReadFile(const fs::path& path) {
		std::ifstream file(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}
}

TEST_CASE("std.fs reading and writing files") {
	TempDirectory dir;

	REQUIRE(RunToString(dir, R"(
		const fs = std.fs;
		const wrote = fs.write("a.txt", "hello");
		const appended = fs.append("a.txt", " world");
		const created = fs.append("new.txt", "x");
		return [wrote, appended, created, fs.read("a.txt"), fs.read("new.txt")];
	)") == "[ true, true, true, hello world, x ]");

	REQUIRE(ReadFile(dir.Root() / "a.txt") == "hello world");

	// write replaces the contents, and binary data survives
	REQUIRE(RunToString(dir, "std.fs.write(\"a.txt\", \"line1\\r\\nline2\\n\"); return std.length(std.fs.read(\"a.txt\"));") == "13");
	REQUIRE(ReadFile(dir.Root() / "a.txt") == "line1\r\nline2\n");

	// failures a script can't prevent are undefined/false, not errors
	REQUIRE(RunToString(dir, "return [std.fs.read(\"missing.txt\"), std.fs.read(\".\"), std.fs.write(\"no/such/dir/x.txt\", \"x\")];")
		== "[ undefined, undefined, false ]");
}
TEST_CASE("std.fs directories") {
	TempDirectory dir;

	REQUIRE(RunToString(dir, R"(
		const fs = std.fs;
		const made = fs.make_dir("sub/deeper");
		fs.write("sub/b.txt", "b");
		fs.write("z.txt", "z");
		return [made, fs.is_dir("sub"), fs.is_file("sub"), fs.is_file("z.txt"), fs.exists("sub/deeper"), fs.exists("nope"),
			fs.list("."), fs.list("sub"), fs.list("missing"), fs.list("z.txt")];
	)") == "[ true, true, false, true, true, false, [ sub, z.txt ], [ b.txt, deeper ], undefined, undefined ]");

	REQUIRE(RunToString(dir, R"(
		const fs = std.fs;
		const renamed = fs.rename("sub/b.txt", "sub/c.txt");
		const removedFull = fs.remove("sub"); // not empty: nothing is removed recursively
		const removedFile = fs.remove("sub/c.txt");
		const removedDir = fs.remove("sub/deeper");
		const removedMissing = fs.remove("sub/deeper");
		return [renamed, removedFull, removedFile, removedDir, removedMissing, fs.list("sub")];
	)") == "[ true, false, true, true, false, [  ] ]");
}
TEST_CASE("std.fs can't leave the root directory") {
	TempDirectory dir;
	{
		std::ofstream secret(dir.Outside() / "secret.txt");
		secret << "secret";
	}

	RequireError(dir, "std.fs.read(\"../secret.txt\");", "outside of the directory that scripts can access");
	RequireError(dir, "std.fs.read(\"sub/../../secret.txt\");", "outside of the directory");
	RequireError(dir, "std.fs.write(\"../written.txt\", \"x\");", "outside of the directory");
	RequireError(dir, "std.fs.list(\"..\");", "outside of the directory");
	RequireError(dir, "std.fs.rename(\"a.txt\", \"../a.txt\");", "outside of the directory");
	REQUIRE_FALSE(fs::exists(dir.Outside() / "written.txt"));

	// an absolute path outside of the root
	const auto absolute = (dir.Outside() / "secret.txt").generic_string();
	RequireError(dir, "std.fs.read(\"" + absolute + "\");", "outside of the directory");

	// ".." that stays inside is fine
	REQUIRE(RunToString(dir, "std.fs.make_dir(\"sub\"); std.fs.write(\"x.txt\", \"x\"); return std.fs.read(\"sub/../x.txt\");") == "x");
	REQUIRE(RunToString(dir, "return std.fs.is_dir(\".\");") == "true");

	RequireError(dir, "std.fs.read(\"\");", "the path is empty");
}
TEST_CASE("std.fs symlinks can't escape the root") {
	TempDirectory dir;
	{
		std::ofstream secret(dir.Outside() / "secret.txt");
		secret << "secret";
	}

	std::error_code ec;
	fs::create_directory_symlink(dir.Outside(), dir.Root() / "link", ec);
	if (ec)
		SKIP("can't create symlinks here: " << ec.message()); // windows without developer mode

	RequireError(dir, "std.fs.read(\"link/secret.txt\");", "outside of the directory");
}
TEST_CASE("the environment decides which parts of std exist") {
	const auto has = [](const std::string& name, const bloop::standard::Environment& environment) {
		const auto ret = bloop::test::TEST_ExecuteBuffer("return std.object.has(std, \"" + name + "\");", environment);
		REQUIRE(ret.has_value());
		return ret->ValueToString() == "true";
	};

	// nothing that reaches outside of the VM by default
	const bloop::standard::Environment locked;
	REQUIRE_FALSE(has("fs", locked));
	REQUIRE_FALSE(has("process", locked));
	REQUIRE(has("console", locked));
	REQUIRE(has("json", locked));
	REQUIRE(has("array", locked));

	auto onlyFiles = locked;
	onlyFiles.m_bFileSystem = true;
	REQUIRE(has("fs", onlyFiles));
	REQUIRE_FALSE(has("process", onlyFiles));

	const auto everything = bloop::standard::Environment::Unrestricted();
	REQUIRE(has("fs", everything));
	REQUIRE(has("process", everything));

	// using a part that doesn't exist terminates the script
	const auto ret = bloop::test::TEST_ExecuteBuffer("return std.fs.read(\"x.txt\");", locked);
	REQUIRE(ret.has_value());
	REQUIRE(ret->type == Value::Type::t_undefined);
	REQUIRE(bloop::test::TEST_LastOutput().find("unknown property \"fs\"") != std::string::npos);
}
