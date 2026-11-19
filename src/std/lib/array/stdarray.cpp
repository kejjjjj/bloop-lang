#include "std/lib/array/stdarray.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

namespace {

	void CheckIndex(bloop::BloopInt index, bloop::BloopInt count) {
		if (index < 0 || index >= count)
			Fail(bloop::fmt::format(BLOOPTEXT("index {} is out of range for an array of {} elements"), index, count));
	}

	// a new array with `count` elements, all set to `fill` (default undefined)
	Value Create(VM& vm, bloop::BloopInt count, std::optional<Value> fill) {
		if (count < 0 || count > MAX_ARRAY_LENGTH)
			Fail(bloop::fmt::format(BLOOPTEXT("can't create an array of {} elements"), count));

		RootScope scope(vm.GetGC());
		const auto value = scope.Keep(fill.value_or(Value{}));

		auto* arr = vm.GetHeap().AllocArray(static_cast<bloop::BloopSize>(count));
		const auto v = value.Get();
		for (bloop::BloopInt i = 0; i < count; i++)
			arr->array.values[i] = v;

		vm.GetGC().WriteBarrierAll(arr); // big arrays start in the old generation
		return Value{ arr };
	}

	// appends the values, returns the new length
	bloop::BloopInt Push(VM& vm, ArrayArg arr, NativeArgs values) {
		// both the array and the values are read from the stack on every iteration (pushing can allocate)
		for (const auto& value : values)
			static_cast<void>(vm.GetHeap().ArrayPush(arr.Obj(), value));
		return arr.Count();
	}

	// removes and returns the last value (undefined if the array is empty)
	Value Pop(ArrayArg arr) {
		auto& a = arr.Obj()->array;
		if (a.count == 0)
			return Value{};

		const auto value = a.values[a.count - 1];
		a.values[--a.count] = Value{};
		return value;
	}

	// inserts a value before `index` (index == length appends), returns the new length
	bloop::BloopInt Insert(VM& vm, ArrayArg arr, bloop::BloopInt index, Value value) {
		if (index < 0 || index > arr.Count())
			Fail(bloop::fmt::format(BLOOPTEXT("index {} is out of range for inserting into an array of {} elements"), index, arr.Count()));

		RootScope scope(vm.GetGC());
		const auto rvalue = scope.Keep(value);
		auto* obj = vm.GetHeap().ArrayReserve(arr.Obj(), arr.Count() + 1);

		auto& a = obj->array;
		for (auto i = a.count; i > index; i--)
			a.values[i] = a.values[i - 1];

		a.values[index] = rvalue.Get();
		a.count++;
		vm.GetGC().WriteBarrier(obj, rvalue.Get());
		return a.count;
	}

	// removes and returns the value at `index`
	Value Remove(ArrayArg arr, bloop::BloopInt index) {
		auto& a = arr.Obj()->array;
		CheckIndex(index, a.count);

		const auto value = a.values[index];
		for (auto i = index; i + 1 < a.count; i++)
			a.values[i] = a.values[i + 1];

		a.values[--a.count] = Value{};
		return value;
	}

	// a copy of the values in [start, end), negative indices count from the end
	Value Slice(VM& vm, ArrayArg arr, std::optional<bloop::BloopInt> start, std::optional<bloop::BloopInt> end) {
		const auto range = ResolveSlice(arr.Count(), start, end);

		auto* result = vm.GetHeap().AllocArray(static_cast<bloop::BloopSize>(range.Count()));
		for (bloop::BloopInt i = 0; i < range.Count(); i++)
			result->array.values[i] = arr[range.begin + i]; // re-read after the allocation

		vm.GetGC().WriteBarrierAll(result);
		return Value{ result };
	}

	// a new array with the values of a followed by the values of b
	Value Concat(VM& vm, ArrayArg a, ArrayArg b) {
		const auto count = a.Count() + b.Count();
		if (count > MAX_ARRAY_LENGTH)
			Fail(bloop::fmt::format(BLOOPTEXT("can't create an array of {} elements"), count));

		auto* result = vm.GetHeap().AllocArray(static_cast<bloop::BloopSize>(count));
		const auto first = a.Count();
		for (bloop::BloopInt i = 0; i < first; i++)
			result->array.values[i] = a[i];
		for (bloop::BloopInt i = 0; i < b.Count(); i++)
			result->array.values[first + i] = b[i];

		vm.GetGC().WriteBarrierAll(result);
		return Value{ result };
	}

	// the index of the first value that is === value, or -1
	bloop::BloopInt IndexOf(ArrayArg arr, Value value) {
		for (bloop::BloopInt i = 0; i < arr.Count(); i++) {
			if (StrictEquals(arr[i], value))
				return i;
		}
		return -1;
	}

	bloop::BloopBool Contains(ArrayArg arr, Value value) {
		return IndexOf(arr, value) != -1;
	}

	// inserts the values at the front (in the given order), returns the new length
	bloop::BloopInt PushFront(VM& vm, ArrayArg arr, NativeArgs values) {
		const auto n = static_cast<bloop::BloopInt>(values.size());
		if (n == 0)
			return arr.Count();

		auto* obj = vm.GetHeap().ArrayReserve(arr.Obj(), arr.Count() + n);
		auto& a = obj->array;

		for (auto i = a.count - 1; i >= 0; i--)
			a.values[i + n] = a.values[i];
		for (bloop::BloopInt i = 0; i < n; i++)
			a.values[i] = values[static_cast<std::size_t>(i)]; // (on the stack, so still valid)

		a.count += n;
		vm.GetGC().WriteBarrierAll(obj);
		return a.count;
	}

	// removes and returns the first value (undefined if the array is empty)
	Value PopFront(ArrayArg arr) {
		if (arr.Count() == 0)
			return Value{};
		return Remove(arr, 0);
	}

	// a new array with callback(element, index) for every element (js map)
	Value Transform(VM& vm, ArrayArg arr, CallableArg callback) {
		RootScope scope(vm.GetGC());
		const auto count = arr.Count();
		const auto result = scope.Keep(vm.GetHeap().AllocArray(static_cast<bloop::BloopSize>(count)));

		// the callback can change the array, so it's read again for every element
		for (bloop::BloopInt i = 0; i < count && i < arr.Count(); i++) {
			const auto value = CallCallback(vm, callback.Get(), { arr[i], Value{ i } });
			result->array.values[i] = value;
			vm.GetGC().WriteBarrier(result.Obj(), value);
		}
		return result.Get();
	}

	// the first (or last) element for which callback(element, index) is truthy
	template<bool Last, bool ReturnIndex>
	Value FindImpl(VM& vm, ArrayArg arr, CallableArg callback) {
		RootScope scope(vm.GetGC());
		auto element = scope.Keep(Value{});

		const auto count = arr.Count();
		for (bloop::BloopInt n = 0; n < count; n++) {
			const auto i = Last ? count - 1 - n : n;
			if (i >= arr.Count())
				continue; // the callback made the array shorter

			element.Set(arr[i]);
			if (CallCallback(vm, callback.Get(), { element.Get(), Value{ i } }).IsTruthy())
				return ReturnIndex ? Value{ i } : element.Get();
		}
		return ReturnIndex ? Value{ bloop::BloopInt{ -1 } } : Value{};
	}
	Value Find(VM& vm, ArrayArg arr, CallableArg callback) { return FindImpl<false, false>(vm, arr, callback); }
	Value FindLast(VM& vm, ArrayArg arr, CallableArg callback) { return FindImpl<true, false>(vm, arr, callback); }
	Value FindIndex(VM& vm, ArrayArg arr, CallableArg callback) { return FindImpl<false, true>(vm, arr, callback); }
	Value FindLastIndex(VM& vm, ArrayArg arr, CallableArg callback) { return FindImpl<true, true>(vm, arr, callback); }

	// a new array with the elements for which callback(element, index) is truthy
	Value Filter(VM& vm, ArrayArg arr, CallableArg callback) {
		RootScope scope(vm.GetGC());
		auto result = scope.Keep(vm.GetHeap().AllocArray(0u));
		auto element = scope.Keep(Value{});

		const auto count = arr.Count();
		for (bloop::BloopInt i = 0; i < count && i < arr.Count(); i++) {
			element.Set(arr[i]);
			if (CallCallback(vm, callback.Get(), { element.Get(), Value{ i } }).IsTruthy())
				result.Set(Value{ vm.GetHeap().ArrayPush(result.Obj(), element.Get()) });
		}
		return result.Get();
	}

	// whether callback(element, index) is truthy for every element (true for an empty array)
	bloop::BloopBool All(VM& vm, ArrayArg arr, CallableArg callback) {
		const auto count = arr.Count();
		for (bloop::BloopInt i = 0; i < count && i < arr.Count(); i++) {
			if (!CallCallback(vm, callback.Get(), { arr[i], Value{ i } }).IsTruthy())
				return false;
		}
		return true;
	}

	// whether callback(element, index) is truthy for any element (false for an empty array)
	bloop::BloopBool Any(VM& vm, ArrayArg arr, CallableArg callback) {
		const auto count = arr.Count();
		for (bloop::BloopInt i = 0; i < count && i < arr.Count(); i++) {
			if (CallCallback(vm, callback.Get(), { arr[i], Value{ i } }).IsTruthy())
				return true;
		}
		return false;
	}

	// calls callback(element, index) for every element
	void ForEach(VM& vm, ArrayArg arr, CallableArg callback) {
		const auto count = arr.Count();
		for (bloop::BloopInt i = 0; i < count && i < arr.Count(); i++)
			static_cast<void>(CallCallback(vm, callback.Get(), { arr[i], Value{ i } }));
	}

	// folds the array: accumulator = callback(accumulator, element, index) (js reduce).
	// Without an initial value, the first element is the initial value
	Value Accumulate(VM& vm, ArrayArg arr, CallableArg callback, std::optional<Value> initial) {
		RootScope scope(vm.GetGC());
		auto accumulator = scope.Keep(initial.value_or(Value{}));

		bloop::BloopInt start = 0;
		if (!initial) {
			if (arr.Count() == 0)
				Fail(BLOOPTEXT("std.array.accumulate: an empty array needs an initial value"));
			accumulator.Set(arr[0]);
			start = 1;
		}

		const auto count = arr.Count();
		for (bloop::BloopInt i = start; i < count && i < arr.Count(); i++)
			accumulator.Set(CallCallback(vm, callback.Get(), { accumulator.Get(), arr[i], Value{ i } }));

		return accumulator.Get();
	}

	// changes the length of the array, new elements are `fill` (default undefined). Returns the array
	ArrayArg Resize(VM& vm, ArrayArg arr, bloop::BloopInt count, std::optional<Value> fill) {
		if (count < 0 || count > MAX_ARRAY_LENGTH)
			Fail(bloop::fmt::format(BLOOPTEXT("can't resize an array to {} elements"), count));

		auto& gc = vm.GetGC();
		RootScope scope(gc);
		const auto value = scope.Keep(fill.value_or(Value{}));

		auto* obj = vm.GetHeap().ArrayReserve(arr.Obj(), count);
		auto& a = obj->array;

		for (auto i = a.count; i < count; i++)
			a.values[i] = value.Get();
		for (auto i = count; i < a.count; i++)
			a.values[i] = Value{}; // [count, capacity) must stay undefined

		a.count = count;
		gc.WriteBarrierAll(obj);
		return arr;
	}

	// sets the elements in [start, end) (default: all of them) to value, returns the array
	ArrayArg Fill(VM& vm, ArrayArg arr, Value value, std::optional<bloop::BloopInt> start, std::optional<bloop::BloopInt> end) {
		const auto range = ResolveSlice(arr.Count(), start, end);
		for (auto i = range.begin; i < range.end; i++)
			arr.Obj()->array.values[i] = value;

		vm.GetGC().WriteBarrier(arr.Obj(), value);
		return arr;
	}

	// reverses the array in place and returns it
	ArrayArg Reverse(ArrayArg arr) {
		auto& a = arr.Obj()->array;
		std::reverse(a.values, a.values + a.count);
		return arr;
	}

	bool IsNumber(const Value& v) noexcept {
		return v.type == Value::Type::t_int || v.type == Value::Type::t_uint || v.type == Value::Type::t_double;
	}
	// a strict weak ordering of numbers (exact for integers, NaN after everything else)
	bool NumberLess(const Value& a, const Value& b) noexcept {
		using VT = Value::Type;
		if (a.type == VT::t_int && b.type == VT::t_int) return a.i < b.i;
		if (a.type == VT::t_uint && b.type == VT::t_uint) return a.u < b.u;
		if (a.type == VT::t_int && b.type == VT::t_uint) return a.i < 0 || static_cast<bloop::BloopUInt>(a.i) < b.u;
		if (a.type == VT::t_uint && b.type == VT::t_int) return b.i >= 0 && a.u < static_cast<bloop::BloopUInt>(b.i);

		const auto toDouble = [](const Value& v) {
			return v.type == VT::t_double ? v.d : v.type == VT::t_int ? static_cast<bloop::BloopDouble>(v.i) : static_cast<bloop::BloopDouble>(v.u);
		};
		const auto x = toDouble(a);
		const auto y = toDouble(b);
		if (std::isnan(x)) return false;
		if (std::isnan(y)) return true;
		return x < y;
	}

	// a stable merge sort that stays memory safe even if `less` is inconsistent
	// (std::sort may read out of bounds with a comparator that isn't a strict weak ordering)
	template<typename Less>
	void MergeSort(std::vector<bloop::BloopInt>& items, Less&& less) {
		std::vector<bloop::BloopInt> buffer(items.size());
		for (std::size_t width = 1; width < items.size(); width *= 2) {
			for (std::size_t lo = 0; lo < items.size(); lo += 2 * width) {
				const auto mid = std::min(lo + width, items.size());
				const auto hi = std::min(lo + 2 * width, items.size());
				auto a = lo, b = mid, out = lo;
				while (a < mid && b < hi)
					buffer[out++] = less(items[b], items[a]) ? items[b++] : items[a++];
				while (a < mid) buffer[out++] = items[a++];
				while (b < hi) buffer[out++] = items[b++];
			}
			items.swap(buffer);
		}
	}

	// sorts with comparator(a, b), which returns a negative number when a comes before b (like js)
	ArrayArg SortWithComparator(VM& vm, ArrayArg arr, CallableArg comparator) {
		auto& gc = vm.GetGC();
		RootScope scope(gc);

		// the comparator can do anything (change the array, collect garbage...), so sort a rooted copy
		const auto count = arr.Count();
		const auto copy = scope.Keep(vm.GetHeap().AllocArray(static_cast<bloop::BloopSize>(count)));
		for (bloop::BloopInt i = 0; i < count; i++)
			copy->array.values[i] = arr[i];
		gc.WriteBarrierAll(copy.Obj());

		std::vector<bloop::BloopInt> order(static_cast<std::size_t>(count));
		for (bloop::BloopInt i = 0; i < count; i++)
			order[static_cast<std::size_t>(i)] = i;

		MergeSort(order, [&](bloop::BloopInt a, bloop::BloopInt b) {
			const auto result = CallCallback(vm, comparator.Get(), { copy->array.values[a], copy->array.values[b] });
			if (!IsNumber(result))
				Fail(bloop::fmt::format(BLOOPTEXT("std.array.sort: the comparator must return a number, but it returned \"{}\""), result.TypeToString()));
			return NumberLess(result, Value{ bloop::BloopInt{ 0 } });
		});

		if (arr.Count() != count)
			Fail(BLOOPTEXT("std.array.sort: the comparator changed the length of the array"));

		for (bloop::BloopInt i = 0; i < count; i++)
			arr.Obj()->array.values[i] = copy->array.values[order[static_cast<std::size_t>(i)]];
		gc.WriteBarrierAll(arr.Obj());
		return arr;
	}

	// sorts in place and returns the array. Without a comparator: an array of numbers or of strings, ascending
	ArrayArg Sort(VM& vm, ArrayArg arr, std::optional<CallableArg> comparator) {
		if (comparator)
			return SortWithComparator(vm, arr, *comparator);

		auto& a = arr.Obj()->array;
		auto* begin = a.values;
		auto* end = a.values + a.count;

		if (std::all_of(begin, end, IsNumber)) {
			std::stable_sort(begin, end, NumberLess);
		} else if (std::all_of(begin, end, [](const Value& v) { return v.IsString(); })) {
			std::stable_sort(begin, end, [](const Value& x, const Value& y) {
				return bloop::BloopStringView(x.obj->string.data, static_cast<std::size_t>(x.obj->string.len))
					< bloop::BloopStringView(y.obj->string.data, static_cast<std::size_t>(y.obj->string.len));
			});
		} else {
			Fail(BLOOPTEXT("std.array.sort only sorts arrays of numbers or arrays of strings"));
		}
		return arr;
	}

	// the values converted to strings, separated by `separator` (default ",")
	bloop::BloopString Join(ArrayArg arr, std::optional<StringArg> separator) {
		const bloop::BloopString sep = separator ? separator->Str() : bloop::BloopString(BLOOPTEXT(","));

		bloop::BloopString result;
		for (bloop::BloopInt i = 0; i < arr.Count(); i++) {
			if (i)
				result += sep;
			result += arr[i].ValueToString();
		}
		return result;
	}
}

NativeDef bloop::standard::GetArrayDefinitions() {
	return NativeDef(BLOOPTEXT("array"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("create"), Bind<&Create>()),
			NativeField(BLOOPTEXT("push"), Bind<&Push>()),
			NativeField(BLOOPTEXT("push_front"), Bind<&PushFront>()),
			NativeField(BLOOPTEXT("pop"), Bind<&Pop>()),
			NativeField(BLOOPTEXT("pop_front"), Bind<&PopFront>()),
			NativeField(BLOOPTEXT("insert"), Bind<&Insert>()),
			NativeField(BLOOPTEXT("remove"), Bind<&Remove>()),
			NativeField(BLOOPTEXT("resize"), Bind<&Resize>()),
			NativeField(BLOOPTEXT("fill"), Bind<&Fill>()),
			NativeField(BLOOPTEXT("slice"), Bind<&Slice>()),
			NativeField(BLOOPTEXT("concat"), Bind<&Concat>()),
			NativeField(BLOOPTEXT("index_of"), Bind<&IndexOf>()),
			NativeField(BLOOPTEXT("contains"), Bind<&Contains>()),
			NativeField(BLOOPTEXT("find"), Bind<&Find>()),
			NativeField(BLOOPTEXT("find_last"), Bind<&FindLast>()),
			NativeField(BLOOPTEXT("find_index"), Bind<&FindIndex>()),
			NativeField(BLOOPTEXT("find_last_index"), Bind<&FindLastIndex>()),
			NativeField(BLOOPTEXT("transform"), Bind<&Transform>()),
			NativeField(BLOOPTEXT("filter"), Bind<&Filter>()),
			NativeField(BLOOPTEXT("all"), Bind<&All>()),
			NativeField(BLOOPTEXT("any"), Bind<&Any>()),
			NativeField(BLOOPTEXT("for_each"), Bind<&ForEach>()),
			NativeField(BLOOPTEXT("accumulate"), Bind<&Accumulate>()),
			NativeField(BLOOPTEXT("reverse"), Bind<&Reverse>()),
			NativeField(BLOOPTEXT("sort"), Bind<&Sort>()),
			NativeField(BLOOPTEXT("join"), Bind<&Join>()),
		}
	});
}
