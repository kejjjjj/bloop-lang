#pragma once
#include "utils/exception.hpp"

namespace bloop::exception {
    struct LexerError : CompileError {
        using CompileError::CompileError;
    };
}
