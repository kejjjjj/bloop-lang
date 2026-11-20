#pragma once
#include <memory>
#include <utility>

#include "vm/gc/gc.hpp"
#include "vm/value.hpp"

#include <cassert>

namespace bloop::vm
{
	class RootScope;

	// A Value that is kept alive by the GC for as long as its RootScope exists.
	// If the value is an object, the GC updates it when the object moves, so always
	// read it through Get() / Obj() after an allocation instead of caching the pointer.
	class Local {
		friend class RootScope;
	public:
		[[nodiscard]] inline Value Get() const noexcept {
			if (m_bObject)
				return Value{ m_pGC->TempRootAt(m_uIndex) };
			return m_oPrimitive;
		}
		[[nodiscard]] inline Object* Obj() const noexcept {
			assert(m_bObject && "Local doesn't hold an object");
			return m_pGC->TempRootAt(m_uIndex);
		}
		[[nodiscard]] inline Object* operator->() const noexcept { return Obj(); }
		[[nodiscard]] inline bool IsObject() const noexcept { return m_bObject; }

		// replaces the rooted value
		inline void Set(const Value& value) noexcept {
			m_bObject = value.type == Value::Type::t_object;
			m_pGC->TempRootAt(m_uIndex) = m_bObject ? value.obj : nullptr;
			m_oPrimitive = m_bObject ? Value{} : value;
		}

	private:
		Local(GC& gc, bloop::BloopSize index, const Value& value) noexcept : m_pGC(&gc), m_uIndex(index) { Set(value); }

		GC* m_pGC;
		bloop::BloopSize m_uIndex;
		Value m_oPrimitive;
		bloop::BloopBool m_bObject{};
	};

	// Roots values for the duration of a C++ scope.
	//
	//   RootScope scope(gc);
	//   auto list = scope.Keep(heap.AllocArray(2));
	//   auto text = scope.Keep(heap.AllocString("hi", 2)); // may move the array
	//   list->array.values[0] = text.Get();                 // re-read through the Local
	//
	// Scopes must be destroyed in reverse order of creation (which C++ does for you),
	// and they also clean up when an exception unwinds the stack.
	class RootScope {
		BLOOP_NONCOPYABLE(RootScope);
	public:
		explicit RootScope(GC& gc) noexcept : m_refGC(gc), m_uBase(gc.TempRootCount()) {}
		~RootScope() { m_refGC.TruncateTempRoots(m_uBase); }

		[[nodiscard]] inline Local Keep(const Value& value) {
			const auto index = m_refGC.TempRootCount();
			m_refGC.PushTempRoot(nullptr);
			return Local(m_refGC, index, value);
		}
		[[nodiscard]] inline Local Keep(Object* obj) {
			assert(obj);
			return Keep(Value{ obj });
		}

	private:
		GC& m_refGC;
		bloop::BloopSize m_uBase;
	};

	// A root that the host owns: it keeps a value alive (and up to date when the GC moves it)
	// across calls into the script, until the handle is destroyed or reset. Move-only.
	// Safe to destroy after the VM is gone (it's just empty then). Make them with GC::MakePersistent
	class Persistent {
	public:
		Persistent() = default;
		Persistent(std::shared_ptr<PersistentTable> table, bloop::BloopSize index) noexcept
			: m_pTable(std::move(table)), m_uIndex(index) {}
		~Persistent() { Reset(); }

		Persistent(Persistent&& other) noexcept : m_pTable(std::move(other.m_pTable)), m_uIndex(other.m_uIndex) {}
		Persistent& operator=(Persistent&& other) noexcept {
			if (this != &other) {
				Reset();
				m_pTable = std::move(other.m_pTable);
				m_uIndex = other.m_uIndex;
			}
			return *this;
		}
		Persistent(const Persistent&) = delete;
		Persistent& operator=(const Persistent&) = delete;

		// false when empty, or when the VM is gone
		[[nodiscard]] bool IsValid() const noexcept { return m_pTable && m_pTable->m_bAlive; }

		// the current value (undefined if invalid). Only valid until the next allocation, like any Value
		[[nodiscard]] Value Get() const noexcept { return IsValid() ? m_pTable->m_oValues[m_uIndex] : Value{}; }

		// true if both hold the same value (the same object for objects)
		[[nodiscard]] bool Holds(const Value& value) const noexcept {
			if (!IsValid())
				return false;
			const auto& mine = m_pTable->m_oValues[m_uIndex];
			if (mine.type != value.type)
				return false;
			return mine.type == Value::Type::t_object ? mine.obj == value.obj : Value(mine).IsEqual(value);
		}

		void Reset() noexcept {
			if (IsValid()) {
				m_pTable->m_oValues[m_uIndex] = Value{};
				m_pTable->m_oFree.push_back(m_uIndex);
			}
			m_pTable.reset();
		}

	private:
		std::shared_ptr<PersistentTable> m_pTable;
		bloop::BloopSize m_uIndex{};
	};

	inline Persistent GC::MakePersistent(const Value& value) {
		auto& table = *m_pPersistent;
		bloop::BloopSize index{};
		if (table.m_oFree.empty()) {
			index = table.m_oValues.size();
			table.m_oValues.push_back(value);
		} else {
			index = table.m_oFree.back();
			table.m_oFree.pop_back();
			table.m_oValues[index] = value;
		}
		return Persistent(m_pPersistent, index);
	}
}
