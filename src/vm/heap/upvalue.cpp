#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

using namespace bloop::vm;

UpValue* GC::CaptureUpValue(Value* slot) {

    UpValue** pp = &m_pOpenUpValues;           // pointer-to-pointer for clean insert
    UpValue* curr = *pp;

    while (curr && curr->location > slot) {
        pp = &curr->next;
        curr = *pp;
    }

    if (curr && curr->location == slot)
        return curr;

    auto up = Allocate<UpValue>(slot, Value{}, curr);
    up->next = curr;
    *pp = up;
    return up;
}
void GC::CloseUpValues(Value* lastSlot)
{
    CheckUpValueList();
    UpValue* uv = m_pOpenUpValues;
    assert(!uv || uv->next == nullptr || uv->location >= uv->next->location);
    while (uv && uv->location >= lastSlot) {
        uv->closed = *uv->location;
        uv->location = &uv->closed;
        uv = uv->next;
    }
    m_pOpenUpValues = uv;
}
void GC::CheckUpValueList() {
    UpValue* uv = m_pOpenUpValues;
    Value* prevLoc = (Value*)UINTPTR_MAX;
    while (uv) {
        if (uv->location >= prevLoc)
            printf("!!! UPVALUE LIST OUT OF ORDER %p >= %p\n", uv->location, prevLoc);
        prevLoc = uv->location;
        uv = uv->next;
    }
}