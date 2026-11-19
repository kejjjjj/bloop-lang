#pragma once
#include "utils/exception.hpp"

namespace bloop::exception {
    struct ParserError : CompileError {
        using CompileError::CompileError;
    };
}
