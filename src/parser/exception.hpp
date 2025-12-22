#pragma once
#include "utils/defs.hpp"

#include <stdexcept>
#include <format>

namespace bloop::exception {
    struct ParserError : std::runtime_error {
        ParserError(const bloop::BloopString& msg) : std::runtime_error(msg) {}
        ParserError(const bloop::BloopString& msg, bloop::CodePosition pos)
            : std::runtime_error(std::format("{}\nat: {}:{}", msg, std::get<0>(pos), std::get<1>(pos))) {}
    };
}