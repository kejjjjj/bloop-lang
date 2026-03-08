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
		[[nodiscard]] inline T* AllocateRaw(bloop::BloopUInt payload_size) {
			return m_pGC->AllocateRaw<T>(payload_size);
		}
		//[[nodiscard]] Object* Allocate(Object* newObj);
		[[nodiscard]] Object* AllocString(const bloop::BloopChar* data, bloop::BloopUInt len);
		[[nodiscard]] Object* AllocString(bloop::BloopUInt len);
		[[nodiscard]] Object* AllocCallable(Function* callable);
		[[nodiscard]] Object* AllocArray(bloop::BloopIndex numValues);
		[[nodiscard]] Object* AllocObject(bloop::BloopIndex numValues);
		[[nodiscard]] Object* AllocClosure(Function* function, bloop::BloopIndex numVals);
		[[nodiscard]] Object* AllocNativeFunction(const bloop::standard::NativeFunction* function);

		[[nodiscard]] Object* StringConcat(Object* a, Object* b);

	private:

		GC* m_pGC{};
	};
}