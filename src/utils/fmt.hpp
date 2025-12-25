#pragma once

#include <string>
#include <vector>

#include "utils/defs.hpp"

namespace bloop::fmt
{
    template<typename T> /*requires std::is_arithmetic_v<T>*/
    [[nodiscard]] inline bloop::BloopString to_string(T val) {

        if constexpr (std::is_convertible_v<T, bloop::BloopString>)
            return val;
        else
            return std::to_string(val);

    }
    template <typename... Args>
    [[nodiscard]] inline bloop::BloopString format(const bloop::BloopString& _fmt, Args&&... args) {

        std::vector<bloop::BloopString> values = { bloop::fmt::to_string(args)... };
        size_t argIndex = 0;
        bloop::BloopString result;

        for (size_t i = 0; i < _fmt.size(); ++i) {
            if (_fmt[i] == BLOOPTEXT('{') && i + 1 < _fmt.size() && _fmt[i + 1] == BLOOPTEXT('}')) {
                if (argIndex < values.size()) {
                    result += values[argIndex++];
                }
                i++;
            }
            else {
                result += bloop::BloopString(1, _fmt[i]);
            }
        }
        return result;
    }

}
