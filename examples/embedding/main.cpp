// a host program that loads a "mod" (mod.bloop), gives it a native module and calls into it.
//   embedding_example [path/to/mod.bloop]

#include "bloop.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {
	struct Game {
		std::vector<std::string> entities;
	};

	// game.spawn(name): returns the number of entities
	bloop::BloopInt Spawn(bloop::vm::VM& vm, bloop::standard::StringArg name) {
		auto* game = vm.GetUserData<Game>();
		game->entities.push_back(name.Str());
		return static_cast<bloop::BloopInt>(game->entities.size());
	}

	bloop::standard::NativeDef GameModule() {
		using namespace bloop::standard;
		return NativeDef("game", NativeObject{ .m_oFields = {
			NativeField("spawn", Bind<&Spawn>()),
			NativeField("name", MakeConstant("example game")),
		}});
	}
}

int main(int argc, char** argv) {
	const std::string path = argc > 1 ? argv[1] : BLOOP_EXAMPLE_MOD;

	Game game;

	bloop::ScriptOptions options;
	options.m_oModules.push_back(GameModule());
	options.m_pUserData = &game;
	options.m_oLimits.m_uInstructions = 1'000'000;
	options.m_oLimits.m_uHeapBytes = 32 * bloop::MB;
	options.m_oEnvironment.m_fnPrint = [](bloop::BloopStringView line) { std::cout << "[mod] " << line << '\n'; };

	bloop::Script mod(options);

	if (auto result = mod.LoadFile(path); !result) {
		std::cout << "the mod didn't compile:\n" << result.m_sError << '\n';
		return 1;
	}
	if (auto result = mod.Run(); !result) {
		std::cout << "the mod failed to start:\n" << result.m_sError << '\n';
		return 1;
	}

	for (auto frame = 0; frame < 3; frame++) {
		if (!mod.HasFunction("on_update"))
			break;

		const auto result = mod.Call("on_update", { frame, 0.016 });
		if (!result) {
			std::cout << "on_update failed:\n" << result.m_sError << '\n';
			break;
		}
		std::cout << "frame " << frame << ": " << result.m_oValue.ToString() << '\n';
	}

	std::cout << "entities: " << game.entities.size() << '\n';
	return 0;
}
