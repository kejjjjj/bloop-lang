#pragma once

#include "utils/defs.hpp"

namespace bloop::vm
{
	class VM;

	struct Object;

	class Heap {
		friend class GC;
	public:
		Heap(VM* vm) : m_pVM(vm){}
		[[nodiscard]] constexpr auto GetAllocatedSize() const noexcept { return m_uBytesAllocated; }
		[[nodiscard]] Object* Allocate(Object* newObj);
		[[nodiscard]] Object* AllocString(char* data, std::size_t len);
		[[nodiscard]] Object* AllocString(std::size_t len);

		[[nodiscard]] Object* StringConcat(Object* a, Object* b);

	private:
		[[nodiscard]] constexpr bool ShouldCollect() const noexcept {
			return m_uBytesAllocated > m_uNextGCLimit;
		}

		// don't call me directly
		void FreeObject(Object* obj);

		Object* m_pObjects{};
		std::size_t m_uBytesAllocated{};
		std::size_t m_uNextGCLimit{ 1024 * 1024 };
		VM* m_pVM{};
	};
}