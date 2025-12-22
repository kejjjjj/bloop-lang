#pragma once
#include "utils/defs.hpp"

#include <stdexcept>
#include <format>

namespace bloop::exception {
    struct LexerError : std::runtime_error {
        LexerError(const bloop::BloopString& msg) : std::runtime_error(msg) {}
        LexerError(const bloop::BloopString& msg, bloop::CodePosition pos) 
            : std::runtime_error(std::format("{}\nat: {}:{}", msg, std::get<0>(pos), std::get<1>(pos))) {}
    };
}