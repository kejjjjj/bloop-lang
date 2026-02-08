#pragma once

#include "utils/defs.hpp"
#include "vm/heap/dvalue.hpp"

#include <vector>
#include <cassert>

namespace bloop::vm
{
	struct Object;
	class Heap;
	class VM;
	struct Value;

	struct alignas(std::max_align_t) GCHeader {

		GCHeader* next{};
		bloop::BloopUInt size{};
		bool marked{};
		bool is_object{};

		[[nodiscard]] static inline GCHeader* GetHeader(void* p) noexcept {
			return reinterpret_cast<GCHeader*>(p) - 1u;
		}
		template<typename T>
		[[nodiscard]] inline T* GetValue() noexcept {
			return reinterpret_cast<T*>(this + 1u);
		}
	};

	static_assert(alignof(GCHeader) >= alignof(std::max_align_t));

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

			constexpr auto header_size = sizeof(GCHeader);
			constexpr auto payload_size = sizeof(T);

			auto mem = static_cast<GCHeader*>(::operator new(header_size + payload_size));
			mem->is_object = std::is_base_of_v<Object, T>;
			mem->size = payload_size;
			mem->marked = false; //this line missing caused heap corruptions.. lol

			auto obj = reinterpret_cast<T*>(mem + 1u); //skip header
			auto result = new (obj) T(std::forward<Args>(args)...);

			mem->next = m_pObjects;
			m_pObjects = mem;

			AddExternalBytes(header_size + payload_size);
			return result;
		}

		void Collect();
		void PushTempRoot(Object* obj);
		void PopTempRoot(bloop::BloopUInt count=1u);
		[[nodiscard]] constexpr auto GetAllocatedSize() const noexcept { return m_uBytesAllocated; }

		inline void AddExternalBytes(bloop::BloopUInt bytes) {
			m_uBytesAllocated += bytes;
		}
		inline void SubtractExternalBytes(bloop::BloopUInt bytes) {
			assert(m_uBytesAllocated >= bytes);
			m_uBytesAllocated -= bytes;
		}

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

#if (DEBUG || _DEBUG)
		void CheckUpValueList();
#endif
		UpValue* m_pOpenUpValues{};

		GCHeader* m_pObjects{};
		VM* m_pVM{};
		std::vector<Object*> m_oTempRoots;

		bloop::BloopUInt m_uBytesAllocated{};
		bloop::BloopUInt m_uNextGCLimit{ 1024 * 1024 };

	};
}