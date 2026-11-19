#pragma once
#include "utils/exception.hpp"

namespace bloop::exception {
    struct ResolverError : CompileError {
        using CompileError::CompileError;
    };
}
