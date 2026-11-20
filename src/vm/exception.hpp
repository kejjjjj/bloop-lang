#pragma once
#include "utils/defs.hpp"

#include <stdexcept>
#include <format>

namespace bloop::exception {
    struct VMError : std::runtime_error {
        VMError(const bloop::BloopString& msg) : std::runtime_error(msg) {}
        VMError(const bloop::BloopString& msg, bloop::CodePosition pos)
            : std::runtime_error(std::format("{}\nat: {}:{}", msg, std::get<0>(pos), std::get<1>(pos))) {}
    };

    // thrown by std.process.exit: ends the script (it's not an error, and scripts can't catch it)
    struct ExitRequest {
        bloop::BloopInt m_iCode{};
    };

    // (internal) an exception that nothing inside a coroutine caught has unwound the coroutine's frames.
    // The VM rethrows it as a script exception where the coroutine was resumed
    struct CoroutineUnwind {};
}