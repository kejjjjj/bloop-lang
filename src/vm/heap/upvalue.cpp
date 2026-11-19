#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"

#include <new>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace bloop::vm;

[[maybe_unused]] constexpr auto MAX_OPEN_UPVALUES = 8192u; // arbitrary large but finite

UpValue* GC::CaptureUpValue(Value* slot) {
    assert(slot != nullptr && "CaptureUpValue called with null slot pointer");

    const auto find = [this, slot](UpValue**& pp) -> UpValue* {
        pp = &m_pOpenUpValues;
        [[maybe_unused]] auto count = 0u;

        // the list is sorted by descending stack location
        for (UpValue* curr = *pp; curr; pp = &curr->next, curr = *pp) {
            assert(count++ < MAX_OPEN_UPVALUES && "Possible cycle or extremely long open upvalue list");
            assert(curr->location != nullptr && "Found upvalue with null location in open list");
            assert(!curr->IsClosed() && "Closed upvalue in the open list");

            if (curr->location == slot)
                return curr;

            if (curr->location < slot)
                break; // insertion point found
        }
        return nullptr;
    };

    UpValue** pp{};
    if (auto* existing = find(pp))
        return existing;

    // Upvalues never move: they live in the old generation from the start.
    // Closures (young or old) can then point to them without any barriers.
    auto* h = AllocOld(sizeof(UpValue), GCKind::upvalue);

    // the allocation may have collected, so look up the insertion point again
    [[maybe_unused]] auto* again = find(pp);
    assert(!again && "upvalue appeared during allocation");

    auto* up = new (h->GetValue<void>()) UpValue{ .location = slot, .closed = Value{}, .next = *pp };
    *pp = up;

    assert(up->location == slot && "Allocated upvalue has wrong location");
    assert(!up->IsClosed() && "New upvalue already marked closed");

#if (DEBUG || _DEBUG)
    CheckUpValueList();
#endif

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
        assert(count++ < MAX_OPEN_UPVALUES && "Extremely long chain being closed - possible cycle?");
        assert(uv->location != nullptr && "Encountered upvalue with null location during close");
        assert(!uv->IsClosed() && "Trying to close already-closed upvalue");

        Value old_value = *uv->location;  // copy before we change pointer
        uv->closed = old_value;
        uv->location = &uv->closed;

        // the upvalue is old, the value it now holds may be young
        WriteBarrier(uv, uv->closed);

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
                std::abort();
            }
        }
    }
}
#endif