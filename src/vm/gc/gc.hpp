#pragma once

#include "utils/defs.hpp"
#include "vm/gc/minor.hpp"
#include "vm/gc/major.hpp"

#include <vector>
#include <cassert>
#include <utility>
#include <new>

namespace bloop::vm
{
	// an object survives this many minor collections in the nursery before it gets promoted
	constexpr bloop::BloopUInt8 PROMOTION_THRESHOLD = 2u;

	// every object in the old generation has this age
	constexpr bloop::BloopUInt8 OLD_AGE = PROMOTION_THRESHOLD;

	constexpr bloop::BloopUInt NURSERY_SIZE = 1u * bloop::MB;              // per semispace
	constexpr bloop::BloopUInt LARGE_OBJECT_SIZE = NURSERY_SIZE / 8u;      // bigger payloads skip the nursery
	constexpr bloop::BloopUInt MAJOR_CHUNK_SIZE = 1u * bloop::MB;          // first old generation chunk
	constexpr bloop::BloopUInt MAJOR_MIN_THRESHOLD = 4u * bloop::MB;       // don't run major GCs on tiny heaps

	// BLOOP_GC_STRESS: run a minor GC on every allocation, and a major GC on every Nth allocation
#if defined(BLOOP_GC_STRESS) && !defined(BLOOP_GC_STRESS_MAJOR_INTERVAL)
#define BLOOP_GC_STRESS_MAJOR_INTERVAL 16u
#endif

	struct Object;
	class Heap;
	class VM;
	struct Value;
	struct UpValue;

	// object:  an Object
	// upvalue: an UpValue (always in the old generation)
	// buffer:  out of line storage owned by exactly one object (the entries of a grown object).
	//          The owner traces its contents, so the buffer itself has no children
	enum class GCKind : bloop::BloopUInt8 { object, upvalue, buffer };

	struct alignas(std::max_align_t) GCHeader {
		bloop::BloopUInt size{};        // payload size in bytes (always aligned), the header is not included
		GCHeader* forwarding{};         // minor GC: where this object was evacuated to
		GCHeader* nextFree{};           // old generation: free list link of a free block
		bloop::BloopUInt8 age{};        // < OLD_AGE while in the nursery
		GCKind kind{ GCKind::object };
		bloop::BloopBool marked{};      // major GC mark bit
		bloop::BloopBool remembered{};  // old object that is in the remembered set
		bloop::BloopBool free{};        // old generation: this block is free memory

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
		template<typename T>
		[[nodiscard]] inline const T* GetValue() const noexcept {
			return reinterpret_cast<const T*>(this + 1u);
		}
		[[nodiscard]] inline bloop::BloopUInt TotalSize() const noexcept { return sizeof(GCHeader) + size; }
		[[nodiscard]] inline bool IsOld() const noexcept { return age >= OLD_AGE; }
		[[nodiscard]] inline bool IsYoung() const noexcept { return age < OLD_AGE; }
	};

	static_assert(alignof(GCHeader) >= alignof(std::max_align_t));
	static_assert(sizeof(GCHeader) % alignof(std::max_align_t) == 0);

	struct GCStats {
		bloop::BloopUInt minorCollections{};
		bloop::BloopUInt majorCollections{};
		bloop::BloopUInt bytesPromoted{};   // nursery -> old generation
		bloop::BloopUInt bytesPretenured{}; // allocated directly into the old generation
		bloop::BloopUInt bytesSurvived{};   // nursery -> nursery
	};

	// Generational garbage collector
	//
	//  - young generation: a semispace copying collector (MinorSpace) with bump allocation.
	//    Survivors get copied to the other semispace, and after PROMOTION_THRESHOLD collections
	//    they get promoted into the old generation.
	//  - old generation: a non-moving mark & sweep heap with a free list (MajorSpace).
	//    Upvalues and large objects are allocated here directly.
	//  - old -> young pointers are tracked with a write barrier that remembers the old *owner*
	//    object, so a minor GC only has to scan the roots, the remembered objects and the survivors.
	//
	// A major GC first evacuates the whole nursery into the old generation, so the mark & sweep
	// only ever has to deal with old objects.
	//
	// Any allocation can move every young object. C++ code holding an Object* across an
	// allocation has to keep it reachable (and re-read it) through PushTempRoot / TempRoot.
	class GC {
		BLOOP_NONCOPYABLE(GC);
		friend class VM;
		friend class Heap;
	public:

		GC() = delete;
		explicit GC(VM* vm);
		~GC();

		template<typename T, typename ... Args>
		[[nodiscard]] T* Allocate(Args&&... args) {
			auto* h = AllocHeader(sizeof(T), GCKind::object);
			return new (h->GetValue<void>()) T(std::forward<Args>(args)...);
		}

		// the payload is left uninitialized, the caller has to initialize it before the next allocation
		template<typename T>
		[[nodiscard]] T* AllocateRaw(bloop::BloopUInt payloadSize) {
			return AllocHeader(payloadSize, GCKind::object)->GetValue<T>();
		}

		// uninitialized storage for `count` Ts, owned by a single object (see GCKind::buffer)
		template<typename T>
		[[nodiscard]] T* AllocateBuffer(bloop::BloopUInt count) {
			return AllocHeader(sizeof(T) * count, GCKind::buffer)->GetValue<T>();
		}

		void MinorGC();
		void MajorGC();

		void PushTempRoot(Object* obj);
		void PopTempRoot(bloop::BloopUInt count = 1u);
		// temp roots get updated when objects move, so always re-read pointers after an allocation
		[[nodiscard]] Object* TempRoot(bloop::BloopUInt fromTop = 0u) const;

		// call after storing `value` into a heap object (or a closed upvalue) `owner`
		inline void WriteBarrier(const void* owner, const Object* value) noexcept;
		inline void WriteBarrier(const void* owner, const Value& value) noexcept; // defined in value.hpp
		// call after (re)initializing many fields of `owner` at once
		inline void WriteBarrierAll(const void* owner) noexcept;

		[[nodiscard]] inline const GCStats& Stats() const noexcept { return m_oStats; }
		[[nodiscard]] inline const MajorSpace& OldSpace() const noexcept { return m_oMajorSpace; }
		[[nodiscard]] inline bloop::BloopUInt NurseryUsed() const noexcept { return m_oMinorSpace.Used(); }
		[[nodiscard]] inline bloop::BloopUInt RememberedSetSize() const noexcept { return m_oRememberedSet.size(); }

		// while one of these is alive, everything gets allocated into the old generation
		// and nothing gets collected (used while the VM builds its constants)
		struct PretenureScope {
			BLOOP_NONCOPYABLE(PretenureScope);
			explicit PretenureScope(GC& gc) : m_refGC(gc) { m_refGC.m_uPretenure++; }
			~PretenureScope() { m_refGC.m_uPretenure--; }
		private:
			GC& m_refGC;
		};

	private:
		[[nodiscard]] GCHeader* AllocHeader(bloop::BloopUInt payloadSize, GCKind kind);
		[[nodiscard]] GCHeader* AllocOld(bloop::BloopUInt payloadSize, GCKind kind);
		[[nodiscard]] inline bool CanCollect() const noexcept { return !m_bCollecting && !m_uPretenure; }
		void MaybeMajorGC();

		void Remember(GCHeader* h);

		//minor
		void MinorCollect(bool tenureAll);
		[[nodiscard]] GCHeader* Evacuate(GCHeader* h);
		void EvacuateObject(Object*& obj);
		template<typename T>
		void EvacuateBuffer(T*& payload) {
			payload = Evacuate(GCHeader::GetHeader(payload))->template GetValue<T>();
		}
		void ScanEvacuated(GCHeader* h);
		void ScanRemembered(GCHeader* h);

		//major
		void MarkRoots();
		void MarkHeader(GCHeader* h);
		void DrainMarkStack();

		template<typename OnSlot>
		void ForEachRoot(OnSlot&& onSlot); // defined in trace.hpp

#ifndef NDEBUG
	public:
		// checks the heap layout and that no old object points into the nursery without being remembered
		[[nodiscard]] bool Verify();
	private:
#endif

		[[maybe_unused]] UpValue* CaptureUpValue(Value* slot);
		void CloseUpValues(Value* lastSlot);
		void WriteUpValue(UpValue* uv, const Value& value);

#if (DEBUG || _DEBUG)
		void CheckUpValueList();
#endif
		UpValue* m_pOpenUpValues{};

		VM* m_pVM{};
		std::vector<Object*> m_oTempRoots;
		std::vector<GCHeader*> m_oRememberedSet;
		std::vector<GCHeader*> m_oWorkList; // gray objects (major) / evacuated objects waiting to be scanned (minor)

		MinorSpace m_oMinorSpace;
		MajorSpace m_oMajorSpace;

		bloop::BloopUInt m_uNextMajorAt{ MAJOR_MIN_THRESHOLD };
		bloop::BloopUInt m_uPretenure{};
		bloop::BloopBool m_bCollecting{};
		bloop::BloopBool m_bTenureAll{};
#ifdef BLOOP_GC_STRESS
		bloop::BloopUInt m_uStressCounter{};
#endif
		GCStats m_oStats;
	};

	inline void GC::WriteBarrier(const void* owner, const Object* value) noexcept {
		assert(owner && value);
		auto* h = const_cast<GCHeader*>(GCHeader::GetHeader(owner));
		if (h->IsYoung() || h->remembered)
			return;
		if (GCHeader::GetHeader(value)->IsOld())
			return;
		Remember(h);
	}
	inline void GC::WriteBarrierAll(const void* owner) noexcept {
		auto* h = const_cast<GCHeader*>(GCHeader::GetHeader(owner));
		if (h->IsOld() && !h->remembered)
			Remember(h);
	}
}
