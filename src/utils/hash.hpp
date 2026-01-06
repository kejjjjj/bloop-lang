#pragma once

#include "utils/defs.hpp"

namespace bloop::hash {

    [[nodiscard]] constexpr auto FNV1a(const BloopChar* s, BloopUInt len) {
        BloopUInt32 h = 2166136261u;
        for (BloopUInt i = 0; i < len; i++) {
            h ^= BloopByte(s[i]);
            h *= 16777619u;
        }
        return h;
    }

}