#pragma once

#include "utils/defs.hpp"
#include "vm/gc/minor.hpp"
#include "vm/gc/major.hpp"

#include <vector>
#include <cassert>
#include <iostream>

namespace bloop::vm
{
	constexpr auto PROMOTION_THRESHOLD = 2;

	struct Object;
	class Heap;
	class VM;
	struct Value;
	struct UpValue;

	struct alignas(std::max_align_t) GCHeader {
		bloop::BloopUInt size{};
		GCHeader* next{}; // linked list for old space
		GCHeader* forwarding{};
		bloop::BloopUInt8 age{};
		bloop::BloopBool marked{};

		[[nodiscard]] static inline GCHeader* GetHeader(void* p) noexcept {
			return reinterpret_cast<GCHeader*>(p) - 1u;
		}
		[[nodiscard]] static inline const GCHeader* GetHeader(const void* p) noexcept {
			return reinterpret_cast<const GCHeader*>(p) - 1u;
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
		friend class Heap;
		friend struct MinorSpace;
	public:

		GC() = delete;
		GC(VM* vm);
		~GC();

		template<typename T, typename ... Args>
		[[nodiscard]] T* Allocate(Args&&... args) {
			constexpr auto header_size = sizeof(GCHeader);
			constexpr auto payload_size = sizeof(T);

			auto mem = static_cast<GCHeader*>(m_oMinorSpace.AllocSpace(header_size + payload_size));

			mem->size = payload_size;
			mem->next = nullptr;
			mem->forwarding = nullptr;
			mem->age = 0;
			mem->marked = false;

			std::cout << "size: " << mem->size << '\n';

			auto obj = reinterpret_cast<T*>(mem + 1u); //skip header
			auto result = new (obj) T(std::forward<Args>(args)...);
			return result;
		}

		template<typename T>
		[[nodiscard]] T* AllocateRaw(bloop::BloopUInt payload_size) {
			constexpr auto header_size = sizeof(GCHeader);

			auto total = bloop::Align(header_size + payload_size);

			auto mem = static_cast<GCHeader*>(m_oMinorSpace.AllocSpace(total));


			mem->size = total - header_size;
			mem->next = nullptr;
			mem->forwarding = nullptr;
			mem->age = 0;
			mem->marked = false;

			std::cout << "size: " << mem->size << '\n';

			auto obj = reinterpret_cast<T*>(mem + 1u); //skip header
			return obj;
		}

		void MinorGC();
		void MinorCopyRoots(VM* vm);
		void MinorScanRememberedSet();

		void MajorGC();
		void PushTempRoot(Object* obj);
		void PopTempRoot(bloop::BloopUInt count=1u);

	private:

		void MarkRoots(VM* vm);
		[[maybe_unused]] Object* Mark(Object* obj);
		void TraceUpValue(UpValue* obj, Object* (GC::* cb)(Object* ptr));

		void Sweep();
		void Trace(Object** slot, Object*(GC::* cb)(Object* ptr));
		[[nodiscard]] Object* TraceMinor(Object* obj);

		[[maybe_unused]] UpValue* CaptureUpValue(Value* slot);
		void CloseUpValues(Value* lastSlot);

#if (DEBUG || _DEBUG)
		void CheckUpValueList();
#endif
		UpValue* m_pOpenUpValues{};

		//GCHeader* m_pObjects{};
		VM* m_pVM{};
		std::vector<Object*> m_oTempRoots;

		MinorSpace m_oMinorSpace;
		MajorSpace m_oMajorSpace;
	};
}