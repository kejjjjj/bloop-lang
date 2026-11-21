#pragma once

#include "utils/defs.hpp"
#include "metadata/metadata.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <memory>
#include <vector>

namespace bloop::ast {
	struct Program;
}

namespace bloop::module {

	struct CompileOptions {
		// imports can only load files inside this directory (or its subdirectories).
		// empty: no limit. A host that runs untrusted scripts should set this, 
		// otherwise an import can read any file that the process can read
		std::filesystem::path m_oImportRoot;

		// the host's own native modules: globals next to std (see bloop.hpp)
		std::vector<bloop::standard::NativeDef> m_oHostModules;

		// reads a file (the main file and the ones it imports): its contents, or nothing if it can't. Empty: from the
		// disk. A host whose files aren't on the disk as they are (a game's archives) reads them itself. The paths are
		// absolute and normalized
		std::function<std::optional<std::string>(const std::filesystem::path&)> m_fnReadFile;
	};

	// lexes, parses, resolves and builds the bytecode of a file and every module it imports (recursively).
	// imports are relative to the file that contains them.
	// the modules run in dependency order before the main file, and each one runs only once
	[[nodiscard]] std::unique_ptr<bloop::ast::Program> CompileFile(const std::filesystem::path& path, bloop::metadata::Metadata& metadata,
		const CompileOptions& options = {});

	// the same for code that isn't in a file: its imports are relative to "directory"
	[[nodiscard]] std::unique_ptr<bloop::ast::Program> CompileSource(bloop::BloopStringView source, bloop::metadata::Metadata& metadata,
		const std::filesystem::path& directory = std::filesystem::current_path(), const bloop::BloopString& name = BLOOPTEXT("main"),
		const CompileOptions& options = {});

}
