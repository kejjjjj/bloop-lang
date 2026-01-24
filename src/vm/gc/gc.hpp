#pragma once

#include "utils/defs.hpp"
#include "vm/heap/dvalue.hpp"

#include <vector>

namespace bloop::vm
{
	struct Object;
	class Heap;
	class VM;
	struct Value;

	struct GCHeader {

		GCHeader* next{};
		bloop::BloopUInt size{};
		bool marked{};
		bool is_object{}; //built in type

		[[nodiscard]] static inline GCHeader* GetHeader(void* p) noexcept {
			return reinterpret_cast<GCHeader*>(p) - 1u;
		}
		template<typename T>
		[[nodiscard]] inline T* GetValue() noexcept {
			return reinterpret_cast<T*>(this + 1u);
		}
	};

	class GC {
		BLOOP_NONCOPYABLE(GC);
		friend class VM;
	public:

		GC() = delete;
		GC(VM* vm);

		template<typename T, typename ... Args>
		T* Allocate(Args&&... args) {

			if (ShouldCollect())
				Collect();

			auto mem = static_cast<GCHeader*>(::operator new(sizeof(GCHeader) + sizeof(T)));
			mem->next = m_pObjects;
			m_pObjects = mem;
			
			auto obj = reinterpret_cast<T*>(mem + 1u); //skip header
			auto result = new (obj) T(std::forward<Args>(args)...);

			if constexpr (std::is_same_v<std::decay_t<Object>, T> && requires{ result->GetSize; }) {
				mem->is_object = true;
				mem->size = result->GetSize();
			} else {
				mem->is_object = false;
				mem->size = sizeof(T);
			}

			m_uBytesAllocated += mem->size;
			return result;
		}

		void Collect();
		void PushTempRoot(Object* obj);
		void PopTempRoot(bloop::BloopUInt count=1u);
		[[nodiscard]] constexpr auto GetAllocatedSize() const noexcept { return m_uBytesAllocated; }

		void Pause() { m_bIsPaused = true; }
		void Continue() { m_bIsPaused = false; }


	private:
		[[nodiscard]] constexpr bool ShouldCollect() const noexcept {
			return m_uBytesAllocated > m_uNextGCLimit;
		}

		// don't call me directly, unless for globals
		void FreeObject(Object* obj);
		void FreeOther(GCHeader* header);

		void MarkRoots(VM* vm);
		void Mark(Object* obj);
		void MarkUpValue(UpValue* obj);

		void Sweep();
		void Trace(Object* obj);

		UpValue* CaptureUpValue(Value* slot);
		void CloseUpValues(Value* lastSlot);

		UpValue* m_pOpenUpValues{};

		GCHeader* m_pObjects{};
		VM* m_pVM{};
		std::vector<Object*> m_oTempRoots;

		bloop::BloopUInt m_uBytesAllocated{};
		bloop::BloopUInt m_uNextGCLimit{ 1024 * 1024 };
		bool m_bIsPaused{};

	};
}