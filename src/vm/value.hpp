#pragma once
#include "utils/defs.hpp"

namespace bloop::vm {

    struct Value {

        enum class Type { t_undefined, t_bool, t_int, t_uint, t_double } type{};
        union { bloop::BloopBool b{}; bloop::BloopInt i; bloop::BloopUInt u; bloop::BloopDouble d; };

        Value(bloop::EValueType t, const bloop::BloopString& data);
        
        Value() : type(Type::t_undefined), b(false){}
        Value(bloop::BloopInt v) : type(Type::t_int), i(v){}

        [[nodiscard]] bool IsTruthy() const;
        [[nodiscard]] bloop::BloopString ValueToString() const;
        [[nodiscard]] bloop::BloopString TypeToString() const;

    };

}