#pragma once
#include "utils/defs.hpp"

#include <stdexcept>
#include <format>

namespace bloop::exception {
    struct ByteCodeError : std::runtime_error {
        ByteCodeError(const bloop::BloopString& msg) : std::runtime_error(msg) {}
        ByteCodeError(const bloop::BloopString& msg, bloop::CodePosition pos)
            : std::runtime_error(std::format("{}\nat: {}:{}", msg, std::get<0>(pos), std::get<1>(pos))) {}
    };
}