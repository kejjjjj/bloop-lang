#pragma once

#include "utils/defs.hpp"
#include "vm/gc/minor.hpp"
#include "vm/gc/major.hpp"

#include <vector>
#include <memory>
#include <cassert>
#include <utility>
#include <new>

namespace bloop::vm
{
	// an object survives this many minor collections in the nursery before it gets promoted
	constexpr bloop::BloopUInt8 PROMOTION_THRESHOLD = 2u;

	// every object in the old generation has this age
	constexpr bloop::BloopUInt8 OLD_AGE = PROMOTION_THRESHOLD;

	constexpr bloop::BloopSize NURSERY_SIZE = 1u * bloop::MB;              // per semispace
	constexpr bloop::BloopSize LARGE_OBJECT_SIZE = NURSERY_SIZE / 8u;      // bigger payloads skip the nursery
	constexpr bloop::BloopSize MAJOR_CHUNK_SIZE = 1u * bloop::MB;          // first old generation chunk
	constexpr bloop::BloopSize MAJOR_MIN_THRESHOLD = 4u * bloop::MB;       // don't run major GCs on tiny heaps

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
		bloop::BloopSize size{};        // payload size in bytes (always aligned), the header is not included
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
		[[nodiscard]] inline bloop::BloopSize TotalSize() const noexcept { return sizeof(GCHeader) + size; }
		[[nodiscard]] inline bool IsOld() const noexcept { return age >= OLD_AGE; }
		[[nodiscard]] inline bool IsYoung() const noexcept { return age < OLD_AGE; }
	};

	static_assert(alignof(GCHeader) >= alignof(std::max_align_t));
	static_assert(sizeof(GCHeader) % alignof(std::max_align_t) == 0);

	struct GCStats {
		bloop::BloopUInt minorCollections{};
		bloop::BloopSize majorCollections{};
		bloop::BloopSize bytesPromoted{};   // nursery -> old generation
		bloop::BloopSize bytesPretenured{}; // allocated directly into the old generation
		bloop::BloopSize bytesSurvived{};   // nursery -> nursery
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
	// the values that the host keeps between calls (see Persistent in handles.hpp). Shared with the handles,
	// so a handle that outlives its VM notices it (m_bAlive) instead of touching freed memory
	class Persistent;

	struct PersistentTable {
		std::vector<Value> m_oValues;
		std::vector<bloop::BloopSize> m_oFree;
		bloop::BloopBool m_bAlive{ true };
	};

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
			return new (h->template GetValue<void>()) T(std::forward<Args>(args)...);
		}

		// the payload is left uninitialized, the caller has to initialize it before the next allocation
		template<typename T>
		[[nodiscard]] T* AllocateRaw(bloop::BloopSize payloadSize) {
			return AllocHeader(payloadSize, GCKind::object)->template GetValue<T>();
		}

		// uninitialized storage for `count` Ts, owned by a single object (see GCKind::buffer)
		template<typename T>
		[[nodiscard]] T* AllocateBuffer(bloop::BloopSize count) {
			return AllocHeader(sizeof(T) * count, GCKind::buffer)->template GetValue<T>();
		}

		void MinorGC();
		void MajorGC();

		// a root that lives until the handle is destroyed (handles.hpp)
		[[nodiscard]] Persistent MakePersistent(const Value& value); // defined in handles.hpp
		[[nodiscard]] const std::shared_ptr<PersistentTable>& Persistents() const noexcept { return m_pPersistent; }

		// 0: no limit. Allocating past the limit (after a full collection) is a runtime error
		void SetHeapLimit(bloop::BloopSize bytes) noexcept { m_uHeapLimit = bytes; }

		// Low level rooting, prefer RootScope / Local (vm/gc/handles.hpp).
		// Temp roots get updated when objects move, so always re-read pointers after an allocation.
		// A null temp root is allowed (it reserves a slot)
		void PushTempRoot(Object* obj);
		void PopTempRoot(bloop::BloopSize count = 1u);
		[[nodiscard]] Object* TempRoot(bloop::BloopSize fromTop = 0u) const;

		[[nodiscard]] inline bloop::BloopSize TempRootCount() const noexcept { return m_oTempRoots.size(); }
		[[nodiscard]] inline Object*& TempRootAt(bloop::BloopSize index) noexcept {
			assert(index < m_oTempRoots.size());
			return m_oTempRoots[index];
		}
		inline void TruncateTempRoots(bloop::BloopSize count) noexcept {
			assert(count <= m_oTempRoots.size() && "temp roots were popped past a RootScope");
			m_oTempRoots.resize(count);
		}

		// call after storing `value` into a heap object (or a closed upvalue) `owner`
		inline void WriteBarrier(const void* owner, const Object* value) noexcept;
		inline void WriteBarrier(const void* owner, const Value& value) noexcept; // defined in value.hpp
		// call after (re)initializing many fields of `owner` at once
		inline void WriteBarrierAll(const void* owner) noexcept;

		[[nodiscard]] inline const GCStats& Stats() const noexcept { return m_oStats; }
		[[nodiscard]] inline const MajorSpace& OldSpace() const noexcept { return m_oMajorSpace; }
		[[nodiscard]] inline bloop::BloopSize NurseryUsed() const noexcept { return m_oMinorSpace.Used(); }
		[[nodiscard]] inline bloop::BloopSize RememberedSetSize() const noexcept { return m_oRememberedSet.size(); }

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
		[[nodiscard]] GCHeader* AllocHeader(bloop::BloopSize payloadSize, GCKind kind);
		[[nodiscard]] GCHeader* AllocOld(bloop::BloopSize payloadSize, GCKind kind);
		[[nodiscard]] inline bool CanCollect() const noexcept { return !m_bCollecting && !m_uPretenure; }
		void MaybeMajorGC();
		// throws a runtime error if the old generation would hold more than the heap limit (after a major GC).
		// only called where an allocation starts, which is a safe point for the GC (and so for unwinding)
		void CheckHeapLimit(bloop::BloopSize extra);

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
		// an upvalue that's closed from the start, holding the value of *source (a root) after the allocation
		[[nodiscard]] UpValue* NewClosedUpValue(const Value* source);
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

		bloop::BloopSize m_uNextMajorAt{ MAJOR_MIN_THRESHOLD };
		bloop::BloopSize m_uHeapLimit{};
		std::shared_ptr<PersistentTable> m_pPersistent{ std::make_shared<PersistentTable>() };
		bloop::BloopSize m_uPretenure{};
		bloop::BloopBool m_bCollecting{};
		bloop::BloopBool m_bTenureAll{};
#ifdef BLOOP_GC_STRESS
		bloop::BloopSize m_uStressCounter{};
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
