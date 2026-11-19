#pragma once
#include "utils/exception.hpp"

namespace bloop::exception {
    struct ByteCodeError : CompileError {
        using CompileError::CompileError;
    };
}
