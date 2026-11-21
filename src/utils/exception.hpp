#pragma once
#include "utils/defs.hpp"
#include "utils/fmt.hpp"

#include <stdexcept>
#include <optional>

namespace bloop::exception {

	// the base of every error thrown before the program runs (lexer, parser, resolver, bytecode).
	// the message and the position are kept separately, so that the module compiler
	// can say which file the position belongs to
    struct CompileError : std::runtime_error {
        CompileError(const bloop::BloopString& msg) : std::runtime_error(msg), m_sMessage(msg) {}
        CompileError(const bloop::BloopString& msg, bloop::CodePosition pos)
            : std::runtime_error(bloop::fmt::format(BLOOPTEXT("{}\nat: {}:{}"), msg, std::get<0>(pos), std::get<1>(pos))),
            m_sMessage(msg), m_oPosition(pos) {}

        bloop::BloopString m_sMessage;
        std::optional<bloop::CodePosition> m_oPosition;
    };
}
