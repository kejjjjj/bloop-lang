#include "std/lib/string/stdstring.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// Strings are sequences of bytes: lengths and indices count bytes, upper/lower only change ASCII letters.

namespace {

	bool IsSpace(char c) noexcept { return std::isspace(static_cast<unsigned char>(c)) != 0; }

	bloop::BloopStringView TrimView(bloop::BloopStringView s) noexcept {
		while (!s.empty() && IsSpace(s.front())) s.remove_prefix(1);
		while (!s.empty() && IsSpace(s.back())) s.remove_suffix(1);
		return s;
	}

	// the bytes in [start, end), negative indices count from the end
	bloop::BloopString Substring(StringArg s, std::optional<bloop::BloopInt> start, std::optional<bloop::BloopInt> end) {
		const auto range = ResolveSlice(s.Length(), start, end);
		return bloop::BloopString(s.View().substr(static_cast<std::size_t>(range.begin), static_cast<std::size_t>(range.Count())));
	}

	// the index of the first occurrence of `part` at or after `from` (default 0), or -1
	bloop::BloopInt Find(StringArg s, StringArg part, std::optional<bloop::BloopInt> from) {
		auto start = from.value_or(0);
		if (start < 0)
			start = 0;
		if (start > s.Length())
			return -1;

		const auto pos = s.View().find(part.View(), static_cast<std::size_t>(start));
		return pos == bloop::BloopStringView::npos ? bloop::BloopInt{ -1 } : static_cast<bloop::BloopInt>(pos);
	}

	// the index of the last occurrence of `part` that starts at or before `from` (default: anywhere), or -1
	bloop::BloopInt FindLast(StringArg s, StringArg part, std::optional<bloop::BloopInt> from) {
		auto start = from.value_or(s.Length());
		if (start < 0)
			return -1;

		const auto pos = s.View().rfind(part.View(), static_cast<std::size_t>(start));
		return pos == bloop::BloopStringView::npos ? bloop::BloopInt{ -1 } : static_cast<bloop::BloopInt>(pos);
	}

	bloop::BloopBool Contains(StringArg s, StringArg part) {
		return s.View().find(part.View()) != bloop::BloopStringView::npos;
	}
	bloop::BloopBool StartsWith(StringArg s, StringArg prefix) {
		return s.View().starts_with(prefix.View());
	}
	bloop::BloopBool EndsWith(StringArg s, StringArg suffix) {
		return s.View().ends_with(suffix.View());
	}

	// splits at every `separator`. An empty separator splits into single characters
	Value Split(VM& vm, StringArg s, StringArg separator) {

		// copy the pieces first: the allocations below can move the arguments
		std::vector<bloop::BloopString> pieces;
		const auto str = s.View();
		const auto sep = separator.View();

		if (sep.empty()) {
			for (const auto c : str)
				pieces.emplace_back(1u, c);
		} else {
			std::size_t begin = 0;
			for (auto pos = str.find(sep); pos != bloop::BloopStringView::npos; pos = str.find(sep, begin)) {
				pieces.emplace_back(str.substr(begin, pos - begin));
				begin = pos + sep.size();
			}
			pieces.emplace_back(str.substr(begin));
		}

		if (static_cast<bloop::BloopInt>(pieces.size()) > MAX_ARRAY_LENGTH)
			Fail(BLOOPTEXT("std.string.split: too many pieces"));

		auto& heap = vm.GetHeap();
		RootScope scope(vm.GetGC());
		const auto result = scope.Keep(heap.AllocArray(pieces.size()));

		for (std::size_t i = 0; i < pieces.size(); i++) {
			auto* piece = heap.AllocString(pieces[i].data(), pieces[i].size());
			result->array.values[i] = Value{ piece };
			vm.GetGC().WriteBarrier(result.Obj(), piece);
		}
		return result.Get();
	}

	// replaces every occurrence of `from` with `to`
	bloop::BloopString Replace(StringArg s, StringArg from, StringArg to) {
		if (from.Length() == 0)
			Fail(BLOOPTEXT("std.string.replace: can't replace an empty string"));

		const auto str = s.View();
		const auto what = from.View();
		bloop::BloopString result;

		std::size_t begin = 0;
		for (auto pos = str.find(what); pos != bloop::BloopStringView::npos; pos = str.find(what, begin)) {
			result += str.substr(begin, pos - begin);
			result += to.View();
			begin = pos + what.size();
		}
		result += str.substr(begin);
		return result;
	}

	bloop::BloopString Clone(StringArg s, bloop::BloopInt count) {
		if (count < 0)
			Fail(bloop::fmt::format(BLOOPTEXT("std.string.clone: can't repeat {} times"), count));
		if (s.Length() && count > std::numeric_limits<bloop::BloopInt>::max() / s.Length())
			Fail(BLOOPTEXT("std.string.clone: the result is too long"));

		bloop::BloopString result;
		result.reserve(static_cast<std::size_t>(s.Length() * count));
		for (bloop::BloopInt i = 0; i < count; i++)
			result += s.View();
		return result;
	}

	bloop::BloopString ToUpper(StringArg s) {
		auto result = s.Str();
		std::transform(result.begin(), result.end(), result.begin(), [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
		return result;
	}
	bloop::BloopString ToLower(StringArg s) {
		auto result = s.Str();
		std::transform(result.begin(), result.end(), result.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
		return result;
	}
	bloop::BloopString Trim(StringArg s) {
		return bloop::BloopString(TrimView(s.View()));
	}

	// the byte at `index` (0 - 255)
	bloop::BloopInt GetCodeAt(StringArg s, bloop::BloopInt index) {
		if (index < 0 || index >= s.Length())
			Fail(bloop::fmt::format(BLOOPTEXT("index {} is out of range for a string of length {}"), index, s.Length()));
		return static_cast<unsigned char>(s.View()[static_cast<std::size_t>(index)]);
	}

	// a string of one byte
	bloop::BloopString FromCharCode(bloop::BloopInt code) {
		if (code < 0 || code > 255)
			Fail(bloop::fmt::format(BLOOPTEXT("std.string.from_char_code: {} is not in the range 0 - 255"), code));
		return bloop::BloopString(1u, static_cast<char>(static_cast<unsigned char>(code)));
	}

	// any value as a string (the same text std.console.log prints)
	bloop::BloopString From(Value value) {
		return value.ValueToString();
	}

	// the whole string (surrounding whitespace allowed) must be an integer, otherwise undefined
	std::optional<bloop::BloopInt> ParseInt(StringArg s) {
		auto text = TrimView(s.View());
		if (!text.empty() && text.front() == '+')
			text.remove_prefix(1);

		bloop::BloopInt value{};
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (text.empty() || error != std::errc{} || end != text.data() + text.size())
			return std::nullopt;
		return value;
	}

	// the whole string (surrounding whitespace allowed) must be a number, otherwise undefined
	std::optional<bloop::BloopDouble> ParseFloat(StringArg s) {
		const auto text = TrimView(s.View());
		if (text.empty())
			return std::nullopt;

		// (not std::from_chars: older libc++ versions don't support it for floating point)
		std::istringstream stream{ bloop::BloopString(text) };
		stream.imbue(std::locale::classic());

		bloop::BloopDouble value{};
		stream >> value;
		if (stream.fail() || stream.peek() != std::char_traits<char>::eof())
			return std::nullopt;
		return value;
	}
}

NativeDef bloop::standard::GetStringDefinitions() {
	return NativeDef(BLOOPTEXT("string"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("substring"), Bind<&Substring>()),
			NativeField(BLOOPTEXT("find"), Bind<&Find>()),
			NativeField(BLOOPTEXT("find_last"), Bind<&FindLast>()),
			NativeField(BLOOPTEXT("contains"), Bind<&Contains>()),
			NativeField(BLOOPTEXT("starts_with"), Bind<&StartsWith>()),
			NativeField(BLOOPTEXT("ends_with"), Bind<&EndsWith>()),
			NativeField(BLOOPTEXT("split"), Bind<&Split>()),
			NativeField(BLOOPTEXT("replace"), Bind<&Replace>()),
			NativeField(BLOOPTEXT("clone"), Bind<&Clone>()),
			NativeField(BLOOPTEXT("to_upper"), Bind<&ToUpper>()),
			NativeField(BLOOPTEXT("to_lower"), Bind<&ToLower>()),
			NativeField(BLOOPTEXT("trim"), Bind<&Trim>()),
			NativeField(BLOOPTEXT("get_code_at"), Bind<&GetCodeAt>()),
			NativeField(BLOOPTEXT("from_char_code"), Bind<&FromCharCode>()),
			NativeField(BLOOPTEXT("from"), Bind<&From>()),
			NativeField(BLOOPTEXT("parse_int"), Bind<&ParseInt>()),
			NativeField(BLOOPTEXT("parse_float"), Bind<&ParseFloat>()),
		}
	});
}
