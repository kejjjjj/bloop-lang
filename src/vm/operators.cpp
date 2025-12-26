#include "vm/value.hpp"
#include "vm/exception.hpp"
#include "vm/heap/dvalue.hpp"
#include "utils/fmt.hpp"

#include <cassert>
#include <array>

using namespace bloop::vm;

using VT = Value::Type;

constexpr std::array<const char*, 6> typeToText = {
    BLOOPTEXT("undefined"),
    BLOOPTEXT("boolean"),
    BLOOPTEXT("uint"),
    BLOOPTEXT("int"),
    BLOOPTEXT("double"),
    BLOOPTEXT("object")
};

void Value::Promote(Value::Type target)
{
    if (type == target) 
        return;

    switch (target) {
    case VT::t_bool:
        b = (type == VT::t_bool) ? b
            : (type == VT::t_uint) ? u != 0
            : (type == VT::t_int) ? i != 0
            : (type == VT::t_double) ? d != 0.0
            : false;
        type = VT::t_bool;
        break;

    case VT::t_uint:
        u = (type == VT::t_bool) ? (b ? 1u : 0u)
            : (type == VT::t_int) ? static_cast<bloop::BloopUInt>(i)
            : (type == VT::t_double) ? static_cast<bloop::BloopUInt>(d)
            : 0u;
        type = VT::t_uint;
        break;

    case VT::t_int:
        i = (type == VT::t_bool) ? (b ? 1 : 0)
            : (type == VT::t_uint) ? static_cast<bloop::BloopInt>(u)
            : (type == VT::t_double) ? static_cast<bloop::BloopInt>(d)
            : 0;
        type = VT::t_int;
        break;

    case VT::t_double:
        d = (type == VT::t_bool) ? (b ? 1.0 : 0.0)
            : (type == VT::t_uint) ? static_cast<bloop::BloopDouble>(u)
            : (type == VT::t_int) ? static_cast<bloop::BloopDouble>(i)
            : 0.0;
        type = VT::t_double;
        break;

    default:
        throw exception::VMError(bloop::fmt::format(
            BLOOPTEXT("cannot promote from \"{}\" to \"{}\""), TypeToString(), typeToText[static_cast<std::size_t>(target)]));
    }
}

constexpr bloop::BloopByte VT_RANK[] = {
    0, // undefined
    1, // bool
    2, // uint
    3, // int
    4, // double
    5  // object (will fail!)
};

void Value::Coerce(Value& v)
{
    if (type == v.type) 
        return;

    VT target = VT_RANK[static_cast<bloop::BloopByte>(type)] > VT_RANK[static_cast<bloop::BloopByte>(v.type)] ? type : v.type;

    Promote(target);
    v.Promote(target);

}

Value Value::operator+(Value v) {

    Coerce(v);

    switch (type) {
    case VT::t_undefined:
        return false;
    case VT::t_bool:
        return static_cast<bloop::BloopBool>(b + v.b);
    case VT::t_uint:
        return u + v.u;
    case VT::t_int:
        return i + v.i;
    case VT::t_double:
        return d + v.d;
    default:
        break;
    }
    throw exception::VMError(bloop::fmt::format(BLOOPTEXT("incompatible operands \"{}\" and \"{}\""), TypeToString(), v.TypeToString()));
}
Value Value::operator-(Value v) {

    Coerce(v);

    switch (type) {
    case VT::t_undefined:
        return false;
    case VT::t_bool:
        return static_cast<bloop::BloopBool>(b - v.b);
    case VT::t_uint:
        return u - v.u;
    case VT::t_int:
        return i - v.i;
    case VT::t_double:
        return d - v.d;
    default:
        break;
    }
    throw exception::VMError(bloop::fmt::format(BLOOPTEXT("incompatible operands \"{}\" and \"{}\""), TypeToString(), v.TypeToString()));
}
Value Value::operator*(Value v) {

    Coerce(v);

    switch (type) {
    case VT::t_bool:
        return static_cast<bloop::BloopBool>(b * v.b);
    case VT::t_uint:
        return u * v.u;
    case VT::t_int:
        return i * v.i;
    case VT::t_double:
        return d * v.d;
    default:
        break;
    }
    throw exception::VMError(bloop::fmt::format(BLOOPTEXT("incompatible operands \"{}\" and \"{}\""), TypeToString(), v.TypeToString()));
}
Value Value::operator/(Value v) {

    Coerce(v);

    switch (type) {
    case VT::t_uint:
        if (v.u == 0)
            throw exception::VMError(BLOOPTEXT("division by 0"));
        return u / v.u;
    case VT::t_int:
        if (v.u == 0)
            throw exception::VMError(BLOOPTEXT("division by 0"));
        return i / v.i;
    case VT::t_double:
        return d / v.d;
    default:
        break;
    }
    throw exception::VMError(bloop::fmt::format(BLOOPTEXT("incompatible operands \"{}\" and \"{}\""), TypeToString(), v.TypeToString()));
}
Value Value::operator<=(Value v) {

    Coerce(v);

    switch (type) {
    case VT::t_undefined:
        return true;
    case VT::t_bool:
        return b <= v.b;
    case VT::t_uint:
        return u <= v.u;
    case VT::t_int:
        return i <= v.i;
    case VT::t_double:
        return d <= v.d;
    default:
        break;
    }
    throw exception::VMError(bloop::fmt::format(BLOOPTEXT("incompatible operands \"{}\" and \"{}\""), TypeToString(), v.TypeToString()));
}