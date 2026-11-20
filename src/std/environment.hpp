#pragma once

#include "utils/defs.hpp"

#include <filesystem>
#include <vector>
#include <functional>

namespace bloop::standard {

	// the parts of the standard library that can reach outside of the VM
	enum class Permission : bloop::BloopByte {
		none,        // always available (console, array, string, object, json...)
		file_system, // std.fs
		process      // std.process (arguments, environment variables, exit)
	};

	// what the standard library may do when a VM runs a script.
	//
	// everything that reaches outside of the VM is off by default, so a host that runs scripts
	// it doesn't trust (game mods...) only gives them what it turns on. The parts that are off
	// don't exist at all: std.fs is missing from std, so using it is a runtime error.
	//
	// the standalone interpreter uses Unrestricted()
	struct Environment {

		bloop::BloopBool m_bFileSystem{};
		bloop::BloopBool m_bProcess{};

		// std.fs paths are relative to this directory and can't leave it (symlinks included).
		// empty: no limit, paths are relative to the working directory
		std::filesystem::path m_oFileSystemRoot;

		// std.process.args
		std::vector<bloop::BloopString> m_oArguments;

		// where std.console.log writes (a line without the newline). Empty: std::cout
		std::function<void(bloop::BloopStringView)> m_fnPrint;

		[[nodiscard]] static Environment Unrestricted() {
			Environment env;
			env.m_bFileSystem = true;
			env.m_bProcess = true;
			return env;
		}

		[[nodiscard]] bool Allows(Permission permission) const noexcept {
			switch (permission) {
			case Permission::file_system: return m_bFileSystem;
			case Permission::process:     return m_bProcess;
			case Permission::none:
			default:                      return true;
			}
		}
	};
}
