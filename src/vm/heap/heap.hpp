#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{
	class VM;
	struct Function;
	struct Value;
	struct Object;
	struct UpValue;

	class Heap {
		friend class GC;
		friend class VM;
	public:
		Heap(VM* vm) : m_pVM(vm){}
		[[nodiscard]] constexpr auto GetAllocatedSize() const noexcept { return m_uBytesAllocated; }
		[[nodiscard]] Object* Allocate(Object* newObj);
		[[nodiscard]] Object* AllocString(bloop::BloopChar* data, bloop::BloopUInt len);
		[[nodiscard]] Object* AllocString(bloop::BloopUInt len);
		[[nodiscard]] Object* AllocCallable(Function* callable);
		[[nodiscard]] Object* AllocArray(bloop::BloopIndex numValues);
		[[nodiscard]] Object* AllocObject(bloop::BloopIndex numValues);
		[[nodiscard]] Object* AllocClosure(Function* function, bloop::BloopIndex numVals);
		[[nodiscard]] Object* AllocUpValue(Value* slot, UpValue* location);

		[[nodiscard]] Object* StringConcat(Object* a, Object* b);

	private:
		[[nodiscard]] constexpr bool ShouldCollect() const noexcept {
			return m_uBytesAllocated > m_uNextGCLimit;
		}

		// don't call me directly, unless for globals
		void FreeObject(Object* obj);

		Object* m_pObjects{};
		std::size_t m_uBytesAllocated{};
		std::size_t m_uNextGCLimit{ 1024 * 1024 };
		VM* m_pVM{};
	};
}