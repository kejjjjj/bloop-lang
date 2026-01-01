#pragma once
#include "utils/defs.hpp"

#include <unordered_map>

namespace bloop::vm {
    struct Object;

    struct Value {

        union { bloop::BloopBool b{}; bloop::BloopInt i; bloop::BloopUInt u; bloop::BloopDouble d; Object* obj; };
        enum class Type : bloop::BloopByte { t_undefined, t_bool, t_uint, t_int, t_double, t_object } type{};

        Value(bloop::EValueType t, const bloop::BloopString& data);
        
        Value(Type t) : type(t){}
        Value() : type(Type::t_undefined), b(false){}
        Value(bloop::BloopBool v) : type(Type::t_bool), b(v) {}
        Value(bloop::BloopUInt v) : type(Type::t_uint), u(v) {}
        Value(bloop::BloopInt v) : type(Type::t_int), i(v){}
        Value(bloop::BloopDouble v) : type(Type::t_double), d(v) {}
        Value(Object* v) : type(Type::t_object), obj(v){}

        [[nodiscard]] bool IsTruthy() const;
        [[nodiscard]] bool IsArithmetic() const;

        [[nodiscard]] bool IsString() const;
        [[nodiscard]] bool IsCallable() const;
        [[nodiscard]] bool IsIndexable() const;
        [[nodiscard]] bloop::BloopInt ToInt() const;

        [[nodiscard]] bloop::BloopString ValueToString() const;
        [[nodiscard]] bloop::BloopString TypeToString() const;
        [[nodiscard]] void Coerce(Value& b); //weaker operand gets modified

        [[nodiscard]] Value operator+(Value b);
        [[nodiscard]] Value operator-(Value b);
        [[nodiscard]] Value operator*(Value b);
        [[nodiscard]] Value operator/(Value b);

        [[nodiscard]] Value operator<=(Value b);

    private:
        void Promote(Value::Type target);
    };


    struct UpValue {
        Object* owner{};
        Value* location{}; //stack slot or &closed
        Value closed; //when stack slot goes out of scope
        UpValue* next{};
    };

}