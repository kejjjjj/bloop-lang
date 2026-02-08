#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

using namespace bloop::vm;

[[maybe_unused]] constexpr auto MAX_OPEN_UPVALUES = 8192u; // arbitrary large but finite

UpValue* GC::CaptureUpValue(Value* slot) {
    assert(slot != nullptr && "CaptureUpValue called with null slot pointer");

    UpValue** pp = &m_pOpenUpValues;
    UpValue* curr = *pp;

    UpValue* seen = nullptr;
    [[maybe_unused]] auto count = 0u;

    while (curr) {
        assert(count++ < MAX_OPEN_UPVALUES && "Possible cycle or extremely long open upvalue list");
        assert(curr->location != nullptr && "Found upvalue with null location in open list");

        if (curr == seen) {
            assert(false && "Cycle detected in open upvalue list during capture");
        }
        seen = curr;

        if (curr->location == slot) {
            assert(curr->location == slot && "location mismatch after compare");
            assert(!curr->IsClosed() && "Reusing closed upvalue as open");
            return curr;
        }

        if (curr->location <= slot) {
            break; // insertion point found
        }

        pp = &curr->next;
        curr = *pp;
    }

    auto up = Allocate<UpValue>(slot, Value{}, curr);
    assert(up != nullptr && "Allocate<UpValue> returned null");
    assert(up->location == slot && "Allocated upvalue has wrong location");
    assert(up->next == curr && "Constructor did not set next correctly");
    assert(!up->IsClosed() && "New upvalue already marked closed");

    up->next = curr;
    *pp = up;

    assert(m_pOpenUpValues != nullptr && "Open list head became null after insert");
    assert(*pp == up && "Insertion via pointer-to-pointer failed");

    return up;
}
void GC::CloseUpValues(Value* lastSlot)
{
    assert(lastSlot != nullptr && "CloseUpValues called with null lastSlot");

#if (DEBUG || _DEBUG)
    CheckUpValueList();
#endif

    UpValue* uv = m_pOpenUpValues;

    if (uv) {
        assert(uv->location != nullptr && "List head has null location");
        assert(!uv->IsClosed() && "List head is already closed");
    }

    assert(!uv || uv->next == nullptr || uv->location >= uv->next->location);
    UpValue* prev = nullptr;
    [[maybe_unused]] auto count = 0u;

    while (uv && uv->location >= lastSlot) {
        assert(count++ < MAX_OPEN_UPVALUES && "Extremely long chain being closed — possible cycle?");
        assert(uv->location != nullptr && "Encountered upvalue with null location during close");
        assert(!uv->IsClosed() && "Trying to close already-closed upvalue");

        Value old_value = *uv->location;  // copy before we change pointer
        uv->closed = old_value;
        uv->location = &uv->closed;

        assert(uv->IsClosed() && "Close did not set location to &closed");
        assert(uv->closed.IsEqual(old_value) && "Closed value was corrupted during assignment");

        prev = uv;
        uv = uv->next;
    }

    m_pOpenUpValues = uv;

    if (prev) {
        assert(prev->next == uv && "Last closed upvalue's next not updated correctly");
    } else {
        assert(m_pOpenUpValues == uv && "List head not updated correctly when closing prefix");
    }

    for (UpValue* rest = m_pOpenUpValues; rest; rest = rest->next) {
        assert(rest->location < lastSlot && "Remaining open upvalue still points >= lastSlot after CloseUpValues");
    }

#if (DEBUG || _DEBUG)
    CheckUpValueList();
#endif
}
#if (DEBUG || _DEBUG)
void GC::CheckUpValueList() {
    UpValue* uv = m_pOpenUpValues;
    Value* prevLoc = reinterpret_cast<Value*>(std::numeric_limits<bloop::BloopUInt>::max());
    auto count = 0u;

    while (uv) {
        assert(count++ < MAX_OPEN_UPVALUES && "Possible cycle or insane open upvalue count");
        assert(uv->location != nullptr && "Null location in open list");

        if (uv->location >= prevLoc) {
            fprintf(stderr, "!!! UPVALUE LIST OUT OF ORDER: %p >= %p (at index %d)\n", (void*)uv->location, (void*)prevLoc, count - 1);
            assert(false && "Open upvalue list is not strictly descending by location");
        }

        assert(!uv->IsClosed() && "Closed upvalue found in open list");

        prevLoc = uv->location;
        uv = uv->next;
    }

    if (m_pOpenUpValues) {
        char* stack_top = reinterpret_cast<char*>(m_pVM->m_oStack.data() + m_pVM->m_oStack.size());
        for (UpValue* u = m_pOpenUpValues; u; u = u->next) {
            if (reinterpret_cast<char*>(u->location) >= stack_top) {
                std::fprintf(stderr, "!!! DANGLING open upvalue %p >= stack top %p\n", u->location, stack_top);
                std::fflush(stderr);
                std::abort();           // or __debugbreak() / breakpoint intrinsic
            }
        }
    }
}
#endif