#pragma once

#include "utils/defs.hpp"
#include "vm/gc/gc.hpp"


namespace bloop::standard {
	struct NativeFunction;
	struct NativeObject;

}

namespace bloop::vm
{
	class VM;
	struct Function;
	struct Value;
	struct Object;
	struct UpValue;


	class Heap {
		BLOOP_NONCOPYABLE(Heap);
		friend class GC;
		friend class VM;
	public:
		Heap(GC* gc) : m_pGC(gc){}

		template<typename T, typename ... Args>
		[[nodiscard]] inline T* Allocate(Args&&... args) {
			return m_pGC->Allocate<T>(std::forward<Args>(args)...);
		}
		template<typename T>
		[[nodiscard]] inline T* AllocateRaw(bloop::BloopSize payload_size) {
			return m_pGC->AllocateRaw<T>(payload_size);
		}
		//[[nodiscard]] Object* Allocate(Object* newObj);
		[[nodiscard]] Object* AllocString(const bloop::BloopChar* data, bloop::BloopSize len);
		[[nodiscard]] Object* AllocString(bloop::BloopSize len);
		[[nodiscard]] Object* AllocCallable(Function* callable);
		[[nodiscard]] Object* AllocArray(bloop::BloopSize numValues);
		[[nodiscard]] Object* AllocObject(bloop::BloopIndex numValues);
		[[nodiscard]] Object* AllocClosure(Function* function, bloop::BloopIndex numVals);
		[[nodiscard]] Object* AllocNativeFunction(const bloop::standard::NativeFunction* function);
		// a suspended coroutine that will run `callee` (a script function or closure, which must be a root).
		// Coroutines are always old: they never move, and the GC frees their state when they die
		[[nodiscard]] Object* AllocCoroutine(const Value& callee);

		[[nodiscard]] Object* StringConcat(Object* a, Object* b);

		// inserts or overwrites a property, growing the object when it gets too full.
		// Growing allocates, so pointers to young objects held by the caller may be stale afterwards
		void ObjectSet(Object* obj, Value key, Value value);

		// makes room for at least `capacity` values. Returns the array (it may have moved)
		[[nodiscard]] Object* ArrayReserve(Object* arr, bloop::BloopInt capacity);
		// appends a value. Returns the array (it may have moved)
		[[maybe_unused]] Object* ArrayPush(Object* arr, Value value);

		[[nodiscard]] inline GC& GetGC() noexcept { return *m_pGC; }

	private:

		GC* m_pGC{};
	};
}