#pragma once

#include "utils/defs.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/gc/minor.hpp"
#include "vm/gc/major.hpp"

#include <vector>
#include <cassert>

namespace bloop::vm
{
	constexpr auto PROMOTION_THRESHOLD = 2;

	struct Object;
	class Heap;
	class VM;
	struct Value;

	struct alignas(std::max_align_t) GCHeader {
		bloop::BloopUInt size{};
		GCHeader* next; // linked list for old space
		GCHeader* forwarding{};
		bloop::BloopUInt8 age{};
		bloop::BloopBool marked{};

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
		friend struct MinorSpace;
	public:

		GC() = delete;
		GC(VM* vm);
		~GC();

		template<typename T, typename ... Args>
		T* Allocate(Args&&... args) {
			constexpr auto header_size = sizeof(GCHeader);
			constexpr auto payload_size = sizeof(T);

			auto mem = static_cast<GCHeader*>(m_oMinorSpace.AllocFromSpace(header_size + payload_size));
			//auto mem = static_cast<GCHeader*>(::operator new(header_size + payload_size));

			mem->size = payload_size;
			mem->marked = false;
			mem->forwarding = nullptr;
			mem->age = 0;

			auto obj = reinterpret_cast<T*>(mem + 1u); //skip header
			auto result = new (obj) T(std::forward<Args>(args)...);

			AddExternalBytes(header_size + payload_size);
			return result;
		}

		void MinorGC();
		void MinorCopyRoots(VM* vm);

		void MajorGC();
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
		// don't call me directly, unless for globals
		void FreeObject(Object* obj);
		void FreeOther(GCHeader* header);

		void MarkRoots(VM* vm);
		[[maybe_unused]] void* Mark(Object* obj);
		void MarkUpValue(UpValue* obj);

		void Sweep();
		void Trace(Object* obj, void*(GC::* cb)(Object* ptr));
		[[nodiscard]] void* TraceMinor(Object* obj);

		UpValue* CaptureUpValue(Value* slot);
		void CloseUpValues(Value* lastSlot);

#if (DEBUG || _DEBUG)
		void CheckUpValueList();
#endif
		UpValue* m_pOpenUpValues{};

		//GCHeader* m_pObjects{};
		VM* m_pVM{};
		std::vector<Object*> m_oTempRoots;

		bloop::BloopUInt m_uBytesAllocated{};

		MinorSpace m_oMinorSpace;
		MajorSpace m_oMajorSpace;
	};
}