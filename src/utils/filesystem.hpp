#pragma once

#include <filesystem>
#include <system_error>

namespace bloop::filesystem {

	// an absolute path without "." and "..", with symlinks resolved as far as the path exists
	[[nodiscard]] inline std::filesystem::path Normalize(const std::filesystem::path& path) {
		std::error_code ec;
		auto result = std::filesystem::weakly_canonical(path, ec);
		if (ec)
			result = std::filesystem::absolute(path, ec).lexically_normal();
		return result;
	}

	// whether `path` is `directory` or somewhere inside it (both normalized)
	[[nodiscard]] inline bool IsInside(const std::filesystem::path& path, const std::filesystem::path& directory) {
		const auto relative = path.lexically_relative(directory);
		if (relative.empty())
			return false; // different roots (another drive on windows)

		const auto first = *relative.begin();
		return first != std::filesystem::path("..");
	}
}
