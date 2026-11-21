#include "module/module.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"
#include "ast/ast.hpp"
#include "ast/module.hpp"
#include "bytecode/build.hpp"
#include "utils/exception.hpp"
#include "utils/fmt.hpp"
#include "utils/filesystem.hpp"

#include <fstream>
#include <iterator>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;
using namespace bloop::module;

namespace {

	using ExportTable = bloop::resolver::internal::ExportTable;

	struct Module {
		fs::path m_oPath; // absolute and normalized, empty for code that isn't in a file
		fs::path m_oDirectory; // imports are relative to this
		bloop::BloopString m_sName; // shown in error messages
		std::unique_ptr<bloop::ast::Program> m_pCode;
		ExportTable m_oExports; // filled in when the module is resolved
		bool m_bLoaded{}; // false while its imports are still being loaded
	};

	[[nodiscard]] std::shared_ptr<const bloop::BloopString> ReadFile(const fs::path& path, const CompileOptions& options) {
		bloop::BloopString source;
		if (options.m_fnReadFile) {
			auto contents = options.m_fnReadFile(path);
			if (!contents)
				return nullptr;
			source = std::move(*contents);
		} else {
			std::ifstream file(path, std::ios::binary);
			if (!file)
				return nullptr;
			source = bloop::BloopString(std::istreambuf_iterator<bloop::BloopChar>(file), std::istreambuf_iterator<bloop::BloopChar>());
		}

		// the utf-8 byte order mark that some editors add
		if (source.starts_with("\xEF\xBB\xBF"))
			source.erase(0, 3);

		return std::make_shared<const bloop::BloopString>(std::move(source));
	}

	using bloop::filesystem::Normalize;

	class CModuleCompiler {
	public:
		CModuleCompiler(bloop::metadata::Metadata& metadata, const fs::path& rootDirectory, const CompileOptions& options)
			: m_refMetadata(metadata), m_oRootDirectory(Normalize(rootDirectory)), m_oOptions(options) {
			if (!options.m_oImportRoot.empty())
				m_oImportRoot = Normalize(options.m_oImportRoot);

			m_refMetadata.m_oNatives = bloop::standard::g_Natives;
			m_refMetadata.m_oNatives.insert(m_refMetadata.m_oNatives.end(), options.m_oHostModules.begin(), options.m_oHostModules.end());
		}

		[[nodiscard]] std::unique_ptr<bloop::ast::Program> Compile(const fs::path& path, const bloop::BloopString& name,
			std::shared_ptr<const bloop::BloopString> source) {

			try {
				auto& main = AddModule(path, name);
				Load(main, std::move(source));
				return Link(main);

			} catch (bloop::exception::CompileError& ex) {

				// a single file keeps the old "line:column" format
				if (m_refMetadata.m_oSourceFiles.size() <= 1u)
					throw;

				if (ex.m_oPosition) {
					bloop::exception::CompileError error(bloop::fmt::format(BLOOPTEXT("{}\nat: {}"), ex.m_sMessage, m_refMetadata.FormatPosition(*ex.m_oPosition)));
					// (the position stays: lines are counted across all files, Metadata::FindSourceFile says whose)
					error.m_sMessage = ex.m_sMessage;
					error.m_oPosition = ex.m_oPosition;
					throw error;
				}

				if (m_pCurrent)
					throw bloop::exception::CompileError(bloop::fmt::format(BLOOPTEXT("{}\nin: {}"), ex.m_sMessage, m_pCurrent->m_sName));

				throw;
			}
		}

	private:
		Module& AddModule(const fs::path& path, const bloop::BloopString& name) {
			auto unit = std::make_unique<Module>();
			unit->m_sName = name;
			unit->m_oDirectory = m_oRootDirectory;

			if (!path.empty()) {
				unit->m_oPath = Normalize(path);
				unit->m_oDirectory = unit->m_oPath.parent_path();
			}

			auto& result = *unit;
			m_oModules.push_back(std::move(unit));
			return result;
		}

		[[nodiscard]] Module* FindModule(const fs::path& path) const {
			for (const auto& m : m_oModules) {
				if (!m->m_oPath.empty() && m->m_oPath == path)
					return m.get();
			}
			return nullptr;
		}

		// how a file is shown in error messages: relative to the main file's directory
		[[nodiscard]] bloop::BloopString DisplayName(const fs::path& path) const {
			auto relative = path.lexically_relative(m_oRootDirectory);
			if (relative.empty())
				relative = path.filename();
			return relative.generic_string();
		}

		void Load(Module& unit, std::shared_ptr<const bloop::BloopString> source) {
			m_pCurrent = &unit;
			m_oLoading.push_back(&unit);

			unit.m_pCode = Parse(unit, std::move(source));

			for (const auto& stmt : unit.m_pCode->m_oStatements) {
				auto* statement = stmt->AsImport();
				if (!statement)
					break; // imports always come first

				statement->m_pExports = &Import(*statement, unit).m_oExports;
				m_pCurrent = &unit;
			}

			m_oLoading.pop_back();
			unit.m_bLoaded = true;
			m_oOrder.push_back(&unit);
		}

		[[nodiscard]] Module& Import(const bloop::ast::ImportStatement& statement, const Module& importer) {
			const auto path = Normalize(importer.m_oDirectory / fs::path(statement.m_sPath));

			if (!m_oImportRoot.empty() && !bloop::filesystem::IsInside(path, m_oImportRoot)) {
				throw bloop::exception::CompileError(bloop::fmt::format(BLOOPTEXT("can't import \"{}\": it's outside of the directory that imports are allowed from"),
					statement.m_sPath), statement.m_oApproximatePosition);
			}

			if (auto* existing = FindModule(path)) {

				if (!existing->m_bLoaded) {
					// still loading its own imports, so it (indirectly) imports this file
					bloop::BloopString chain;
					const auto start = std::ranges::find(m_oLoading, existing);
					for (auto itr = start; itr != m_oLoading.end(); ++itr)
						chain += (*itr)->m_sName + BLOOPTEXT(" -> ");
					chain += existing->m_sName;

					throw bloop::exception::CompileError(bloop::fmt::format(BLOOPTEXT("circular import: {}"), chain), statement.m_oApproximatePosition);
				}

				return *existing; // already loaded (and it'll run only once)
			}

			auto source = ReadFile(path, m_oOptions);
			if (!source) {
				throw bloop::exception::CompileError(bloop::fmt::format(BLOOPTEXT("couldn't open the module \"{}\" ({})"), 
					statement.m_sPath, path.generic_string()), statement.m_oApproximatePosition);
			}

			auto& unit = AddModule(path, DisplayName(path));
			Load(unit, std::move(source));
			return unit;
		}

		[[nodiscard]] std::unique_ptr<bloop::ast::Program> Parse(const Module& unit, std::shared_ptr<const bloop::BloopString> source) {
			assert(source);

			// every file shares the same line map, so this file's lines start where the previous file's ended
			const auto firstLine = m_refMetadata.m_oLineMap.size() + 1u;
			m_refMetadata.m_oSourceFiles.push_back({ unit.m_sName, source, firstLine });

			if (source->empty()) {
				m_refMetadata.m_oLineMap.emplace_back();
				return std::make_unique<bloop::ast::Program>(bloop::CodePosition{ firstLine, 1u });
			}

			bloop::lexer::CLexer lexer(*source, m_refMetadata);
			lexer.Parse();

			if (lexer.GetTokens().empty()) // only comments
				return std::make_unique<bloop::ast::Program>(bloop::CodePosition{ firstLine, 1u });

			bloop::parser::CLexParser parser(lexer, m_refMetadata);
			auto code = parser.Parse();
			if (!code)
				throw bloop::exception::CompileError(BLOOPTEXT("failed to parse the file"));

			return code;
		}

		// resolves every module (dependencies first) and puts them together into one program
		[[nodiscard]] std::unique_ptr<bloop::ast::Program> Link(Module& main) {
			bloop::resolver::internal::Resolver resolver(m_refMetadata);
			resolver.BeginProgram();

			for (auto* unit : m_oOrder) {
				m_pCurrent = unit;
				resolver.ResolveModule(unit->m_pCode.get(), unit != &main, &unit->m_oExports);
			}

			auto program = std::make_unique<bloop::ast::Program>(main.m_pCode->m_oApproximatePosition);

			// all modules run in one global chunk, in dependency order
			for (auto* unit : m_oOrder) {
				for (auto& stmt : unit->m_pCode->m_oStatements)
					program->AddStatement(std::move(stmt));
				unit->m_pCode->m_oStatements.clear();
			}

			resolver.EndProgram(program.get());

			for (const auto& [name, symbol] : main.m_oExports)
				m_refMetadata.m_oExports[name] = symbol->m_uSlot;

			m_pCurrent = nullptr;
			bloop::bytecode::BuildByteCode(program.get(), m_refMetadata);
			return program;
		}

		bloop::metadata::Metadata& m_refMetadata;
		fs::path m_oRootDirectory;
		fs::path m_oImportRoot; // empty: anywhere
		const CompileOptions& m_oOptions; // (outlives the compiler: the caller's)

		std::vector<std::unique_ptr<Module>> m_oModules;
		std::vector<Module*> m_oOrder; // dependencies before the modules that import them
		std::vector<Module*> m_oLoading; // the import chain that's being loaded right now
		Module* m_pCurrent{}; // for errors that don't have a position
	};
}

std::unique_ptr<bloop::ast::Program> bloop::module::CompileFile(const fs::path& path, bloop::metadata::Metadata& metadata, const CompileOptions& options) {
	const auto normalized = Normalize(path);
	auto source = ReadFile(normalized, options);
	if (!source)
		throw bloop::exception::CompileError(bloop::fmt::format(BLOOPTEXT("couldn't open \"{}\""), path.generic_string()));

	CModuleCompiler compiler(metadata, normalized.parent_path(), options);
	return compiler.Compile(normalized, normalized.filename().generic_string(), std::move(source));
}

std::unique_ptr<bloop::ast::Program> bloop::module::CompileSource(bloop::BloopStringView source, bloop::metadata::Metadata& metadata,
	const fs::path& directory, const bloop::BloopString& name, const CompileOptions& options) {

	CModuleCompiler compiler(metadata, directory, options);
	return compiler.Compile({}, name, std::make_shared<const bloop::BloopString>(source));
}
