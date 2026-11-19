#include "tests/vm/defs.hpp"

#include <filesystem>
#include <string>

// the tools written in bloop (tools/ in the repository) keep working

TEST_CASE("tools | stats") {
	const auto fixture = (std::filesystem::path(SOURCE_DIR) / "vm" / "tools" / "stats_fixture").generic_string();

	auto environment = bloop::standard::Environment::Unrestricted();
	environment.m_oArguments = { fixture, "--json" };

	const auto ret = bloop::test::TEST_ExecuteFile(JOIN_PATH("..", "..") + BLOOP_DIRECTORY_SEPARATOR + JOIN_PATH("tools", "stats")
		+ BLOOP_DIRECTORY_SEPARATOR + "main.bloop", environment);

	const auto& output = bloop::test::TEST_LastOutput();
	INFO(output);
	REQUIRE(ret.has_value());
	REQUIRE(bloop::test::TEST_GetLastVM()->GetExitCode() == 0);

	// .hidden/ and node_modules/ are skipped, "toDouble" isn't a todo
	REQUIRE(output.find("\".h\": {\n      \"files\": 1,\n      \"lines\": 4,\n      \"blank\": 1,\n      \"comments\": 1\n    }") != std::string::npos);
	REQUIRE(output.find("\".bloop\": {\n      \"files\": 1,\n      \"lines\": 2,\n      \"blank\": 1,\n      \"comments\": 0\n    }") != std::string::npos);
	REQUIRE(output.find("\".txt\": {\n      \"files\": 1,\n      \"lines\": 1,") != std::string::npos);
	REQUIRE(output.find("\"path\": \"sub/c.txt\"") != std::string::npos);
	REQUIRE(output.find("\"line\": 3,\n      \"text\": \"int main() { return 0; } // todo: something\"") != std::string::npos);
	REQUIRE(output.find("toDouble") == std::string::npos);
	REQUIRE(output.find("x.h") == std::string::npos);
	REQUIRE(output.find("y.h") == std::string::npos);
}
