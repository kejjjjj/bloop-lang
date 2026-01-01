#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

using namespace bloop::vm;

UpValue* VM::CaptureUpValue(Value* slot) {

	UpValue* prev = nullptr;
	UpValue* curr = m_pOpenUpValues;

    while (curr && curr->location > slot) {
        prev = curr;
        curr = curr->next;
    }

    if (curr && curr->location == slot)
        return curr;

    auto up = m_oHeap.AllocUpValue(slot, curr);
    if (prev) prev->next = up->upvalue;
    else m_pOpenUpValues = up->upvalue;
    return up->upvalue;
}
void VM::CloseUpValues(Value* lastSlot)
{
    while (m_pOpenUpValues && m_pOpenUpValues->location >= lastSlot) {
        m_pOpenUpValues->closed = *m_pOpenUpValues->location;
        m_pOpenUpValues->location = &m_pOpenUpValues->closed;
        m_pOpenUpValues = m_pOpenUpValues->next;
    }
}