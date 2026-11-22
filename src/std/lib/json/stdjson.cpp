#include "std/lib/json/stdjson.hpp"
#include "std/lib/common.hpp"
#include "vm/heap/heap.hpp"
#include "utils/fmt.hpp"

#include <algorithm>
#include <ranges>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace bloop::vm;
using namespace bloop::standard;
using namespace bloop::standard::lib;

// std.json: converting values to json text and back.
//   json   -> bloop: null -> undefined, numbers without a fraction or an exponent -> int (double if it doesn't fit),
//                    other numbers -> double, objects keep the order of their keys (the last duplicate wins)
//   bloop  -> json:  undefined -> null, int/uint/double -> number, keys are written in insertion order.
//                    functions, NaN, infinities and values that contain themselves can't be converted

namespace {

	// deeper nesting than this is an error (both ways), so that the recursion can't overflow the C++ stack
	constexpr std::size_t MAX_DEPTH = 512u;

	// ======================== stringify ========================

	// the shortest text that reads back as the same double (independent of the C++ locale). Not in exponent form while
	// plain digits are reasonable (10 is "10", not "1e+01")
	[[nodiscard]] std::string FormatDouble(double value) {
		const double magnitude = std::fabs(value);
		const bool plainRange = magnitude >= 1e-5 && magnitude < 1e15;

		for (int precision = 1; precision <= 17; precision++) {
			std::ostringstream out;
			out.imbue(std::locale::classic());
			out.precision(precision);
			out << value;

			std::istringstream in(out.str());
			in.imbue(std::locale::classic());
			double parsed{};
			in >> parsed;

			const bool exponent = out.str().find('e') != std::string::npos;
			if ((parsed == value && !(exponent && plainRange)) || precision == 17)
				return out.str();
		}
		return {};
	}

	class CJsonWriter {
	public:
		explicit CJsonWriter(bloop::BloopInt indent) : m_iIndent(indent) {}

		void Write(const Value& value) {
			switch (value.type) {
			case Value::Type::t_undefined:
				m_sOut += "null";
				return;
			case Value::Type::t_bool:
				m_sOut += value.b ? "true" : "false";
				return;
			case Value::Type::t_int:
				m_sOut += std::to_string(value.i);
				return;
			case Value::Type::t_uint:
				m_sOut += std::to_string(value.u);
				return;
			case Value::Type::t_double:
				if (!std::isfinite(value.d))
					Fail(BLOOPTEXT("std.json.stringify: NaN and infinity can't be converted to json"));
				m_sOut += FormatDouble(value.d);
				return;
			case Value::Type::t_object:
				break;
			}

			const auto* obj = value.obj;
			if (obj->IsString()) {
				WriteString({ obj->string.data, static_cast<std::size_t>(obj->string.len) });
				return;
			}
			if (!obj->IsArray() && !obj->IsObject())
				Fail(bloop::fmt::format(BLOOPTEXT("std.json.stringify: a value of type \"{}\" can't be converted to json"), value.TypeToString()));

			if (std::ranges::find(m_oPath, obj) != m_oPath.end())
				Fail(BLOOPTEXT("std.json.stringify: the value contains itself"));
			if (m_oPath.size() >= MAX_DEPTH)
				Fail(bloop::fmt::format(BLOOPTEXT("std.json.stringify: the value is nested more than {} levels deep"), MAX_DEPTH));

			m_oPath.push_back(obj);
			if (obj->IsArray())
				WriteArray(obj);
			else
				WriteObject(obj);
			m_oPath.pop_back();
		}

		[[nodiscard]] std::string& Result() noexcept { return m_sOut; }

	private:
		void WriteArray(const Object* arr) {
			const auto count = arr->array.count;
			if (count == 0) {
				m_sOut += "[]";
				return;
			}

			m_sOut += '[';
			for (bloop::BloopInt i = 0; i < count; i++) {
				if (i)
					m_sOut += ',';
				NewLine(m_oPath.size());
				Write(arr->array.values[i]);
			}
			NewLine(m_oPath.size() - 1u);
			m_sOut += ']';
		}

		void WriteObject(const Object* obj) {
			const auto slots = obj->ObjectSlotsInOrder();
			if (slots.empty()) {
				m_sOut += "{}";
				return;
			}

			m_sOut += '{';
			for (std::size_t i = 0; i < slots.size(); i++) {
				const auto& entry = obj->object.entries[slots[i]];
				if (i)
					m_sOut += ',';
				NewLine(m_oPath.size());
				if (!entry.key.IsString())
					Fail(bloop::fmt::format(BLOOPTEXT("std.json.stringify: an object with a handle ({}) as a key can't be converted to json"), entry.key.ValueToString()));
				WriteString({ entry.key.obj->string.data, static_cast<std::size_t>(entry.key.obj->string.len) });
				m_sOut += m_iIndent > 0 ? ": " : ":";
				Write(entry.value);
			}
			NewLine(m_oPath.size() - 1u);
			m_sOut += '}';
		}

		void NewLine(std::size_t depth) {
			if (m_iIndent <= 0)
				return;
			m_sOut += '\n';
			m_sOut.append(depth * static_cast<std::size_t>(m_iIndent), ' ');
		}

		void WriteString(std::string_view str) {
			constexpr char hex[] = "0123456789abcdef";

			m_sOut += '"';
			for (const auto c : str) {
				switch (c) {
				case '"':  m_sOut += "\\\""; break;
				case '\\': m_sOut += "\\\\"; break;
				case '\b': m_sOut += "\\b"; break;
				case '\f': m_sOut += "\\f"; break;
				case '\n': m_sOut += "\\n"; break;
				case '\r': m_sOut += "\\r"; break;
				case '\t': m_sOut += "\\t"; break;
				default:
					if (static_cast<unsigned char>(c) < 0x20u) {
						m_sOut += "\\u00";
						m_sOut += hex[(static_cast<unsigned char>(c) >> 4) & 0xFu];
						m_sOut += hex[static_cast<unsigned char>(c) & 0xFu];
					} else {
						m_sOut += c; // utf-8 passes through as it is
					}
				}
			}
			m_sOut += '"';
		}

		std::string m_sOut;
		bloop::BloopInt m_iIndent{};
		std::vector<const Object*> m_oPath; // the arrays and objects that are being written
	};

	// json text of a value. indent > 0 puts every element on its own line, indented by that many spaces per level
	bloop::BloopString Stringify(Value value, std::optional<bloop::BloopInt> indent) {
		if (indent && (*indent < 0 || *indent > 16))
			Fail(BLOOPTEXT("std.json.stringify: the indent must be between 0 and 16"));

		CJsonWriter writer(indent.value_or(0));
		writer.Write(value);
		return std::move(writer.Result());
	}

	// ======================== parse ========================

	// the parsed document, before any of it is allocated in the VM
	struct JsonNode {
		enum class Kind : bloop::BloopByte { null, boolean, integer, number, string, array, object } kind{ Kind::null };
		bool boolean{};
		bloop::BloopInt integer{};
		double number{};
		std::string string;
		std::vector<JsonNode> items;    // the elements of an array, the values of an object
		std::vector<std::string> keys;  // the keys of an object (same order as items)
	};

	// strict json (RFC 8259): no comments, no trailing commas, no single quotes
	class CJsonParser {
	public:
		explicit CJsonParser(std::string_view text) : m_sText(text) {}

		// false if the text isn't valid json
		[[nodiscard]] bool Parse(JsonNode& result) {
			SkipWhiteSpace();
			if (!ParseValue(result, 0u))
				return false;
			SkipWhiteSpace();
			return m_uPos == m_sText.size();
		}

	private:
		[[nodiscard]] bool AtEnd() const noexcept { return m_uPos >= m_sText.size(); }
		[[nodiscard]] char Peek() const noexcept { return AtEnd() ? '\0' : m_sText[m_uPos]; }

		void SkipWhiteSpace() noexcept {
			while (!AtEnd() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\n' || Peek() == '\r'))
				m_uPos++;
		}

		[[nodiscard]] bool Consume(std::string_view word) noexcept {
			if (m_sText.substr(m_uPos, word.size()) != word)
				return false;
			m_uPos += word.size();
			return true;
		}

		[[nodiscard]] bool ParseValue(JsonNode& node, std::size_t depth) {
			if (depth >= MAX_DEPTH)
				return false;

			switch (Peek()) {
			case '{': return ParseObject(node, depth);
			case '[': return ParseArray(node, depth);
			case '"':
				node.kind = JsonNode::Kind::string;
				return ParseString(node.string);
			case 't':
				node.kind = JsonNode::Kind::boolean;
				node.boolean = true;
				return Consume("true");
			case 'f':
				node.kind = JsonNode::Kind::boolean;
				node.boolean = false;
				return Consume("false");
			case 'n':
				node.kind = JsonNode::Kind::null;
				return Consume("null");
			default:
				return ParseNumber(node);
			}
		}

		[[nodiscard]] bool ParseArray(JsonNode& node, std::size_t depth) {
			node.kind = JsonNode::Kind::array;
			m_uPos++; // [
			SkipWhiteSpace();

			if (Peek() == ']') {
				m_uPos++;
				return true;
			}

			while (true) {
				SkipWhiteSpace();
				if (!ParseValue(node.items.emplace_back(), depth + 1u))
					return false;
				if (static_cast<bloop::BloopInt>(node.items.size()) > MAX_ARRAY_LENGTH)
					return false;
				SkipWhiteSpace();

				if (Peek() == ',') {
					m_uPos++;
					continue;
				}
				if (Peek() == ']') {
					m_uPos++;
					return true;
				}
				return false;
			}
		}

		[[nodiscard]] bool ParseObject(JsonNode& node, std::size_t depth) {
			node.kind = JsonNode::Kind::object;
			m_uPos++; // {
			SkipWhiteSpace();

			if (Peek() == '}') {
				m_uPos++;
				return true;
			}

			while (true) {
				SkipWhiteSpace();
				if (Peek() != '"')
					return false;

				if (!ParseString(node.keys.emplace_back()))
					return false;

				SkipWhiteSpace();
				if (Peek() != ':')
					return false;
				m_uPos++;
				SkipWhiteSpace();

				if (!ParseValue(node.items.emplace_back(), depth + 1u))
					return false;
				SkipWhiteSpace();

				if (Peek() == ',') {
					m_uPos++;
					continue;
				}
				if (Peek() == '}') {
					m_uPos++;
					return true;
				}
				return false;
			}
		}

		[[nodiscard]] bool ParseHex4(std::uint32_t& value) {
			if (m_uPos + 4u > m_sText.size())
				return false;
			const auto* begin = m_sText.data() + m_uPos;
			const auto [end, ec] = std::from_chars(begin, begin + 4, value, 16);
			if (ec != std::errc{} || end != begin + 4)
				return false;
			m_uPos += 4u;
			return true;
		}

		static void AppendUtf8(std::string& out, std::uint32_t cp) {
			if (cp < 0x80u) {
				out += static_cast<char>(cp);
			} else if (cp < 0x800u) {
				out += static_cast<char>(0xC0u | (cp >> 6));
				out += static_cast<char>(0x80u | (cp & 0x3Fu));
			} else if (cp < 0x10000u) {
				out += static_cast<char>(0xE0u | (cp >> 12));
				out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
				out += static_cast<char>(0x80u | (cp & 0x3Fu));
			} else {
				out += static_cast<char>(0xF0u | (cp >> 18));
				out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
				out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
				out += static_cast<char>(0x80u | (cp & 0x3Fu));
			}
		}

		[[nodiscard]] bool ParseString(std::string& out) {
			m_uPos++; // "

			while (true) {
				if (AtEnd())
					return false;

				const auto c = m_sText[m_uPos++];
				if (c == '"')
					return true;
				if (static_cast<unsigned char>(c) < 0x20u)
					return false; // control characters must be escaped

				if (c != '\\') {
					out += c;
					continue;
				}

				if (AtEnd())
					return false;

				switch (m_sText[m_uPos++]) {
				case '"':  out += '"'; break;
				case '\\': out += '\\'; break;
				case '/':  out += '/'; break;
				case 'b':  out += '\b'; break;
				case 'f':  out += '\f'; break;
				case 'n':  out += '\n'; break;
				case 'r':  out += '\r'; break;
				case 't':  out += '\t'; break;
				case 'u': {
					std::uint32_t cp{};
					if (!ParseHex4(cp))
						return false;

					if (cp >= 0xD800u && cp <= 0xDBFFu) {
						// a surrogate pair: 😀
						std::uint32_t low{};
						if (!Consume("\\u") || !ParseHex4(low) || low < 0xDC00u || low > 0xDFFFu)
							return false;
						cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
					} else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
						return false; // a low surrogate without a high one
					}

					AppendUtf8(out, cp);
					break;
				}
				default:
					return false;
				}
			}
		}

		[[nodiscard]] bool ParseNumber(JsonNode& node) {
			const auto start = m_uPos;
			const auto Digits = [this] {
				const auto begin = m_uPos;
				while (!AtEnd() && Peek() >= '0' && Peek() <= '9')
					m_uPos++;
				return m_uPos - begin;
			};

			if (Peek() == '-')
				m_uPos++;

			// no leading zeros (except a single 0)
			if (Peek() == '0') {
				m_uPos++;
			} else if (Digits() == 0u) {
				return false;
			}

			bool isInteger = true;
			if (Peek() == '.') {
				m_uPos++;
				isInteger = false;
				if (Digits() == 0u)
					return false;
			}
			if (Peek() == 'e' || Peek() == 'E') {
				m_uPos++;
				isInteger = false;
				if (Peek() == '+' || Peek() == '-')
					m_uPos++;
				if (Digits() == 0u)
					return false;
			}

			const auto text = m_sText.substr(start, m_uPos - start);

			if (isInteger) {
				bloop::BloopInt value{};
				const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
				if (ec == std::errc{} && end == text.data() + text.size()) {
					node.kind = JsonNode::Kind::integer;
					node.integer = value;
					return true;
				}
				// too big for an int: a double instead
			}

			std::istringstream in{ std::string(text) };
			in.imbue(std::locale::classic());
			double value{};
			in >> value;
			if (in.fail())
				value = text.front() == '-' ? -HUGE_VAL : HUGE_VAL; // out of range

			node.kind = JsonNode::Kind::number;
			node.number = value;
			return true;
		}

		std::string_view m_sText;
		std::size_t m_uPos{};
	};

	// allocates the parsed document in the VM
	[[nodiscard]] Value ToValue(VM& vm, const JsonNode& node) {
		auto& heap = vm.GetHeap();

		switch (node.kind) {
		case JsonNode::Kind::null:    return Value{};
		case JsonNode::Kind::boolean: return Value{ static_cast<bloop::BloopBool>(node.boolean) };
		case JsonNode::Kind::integer: return Value{ node.integer };
		case JsonNode::Kind::number:  return Value{ static_cast<bloop::BloopDouble>(node.number) };
		case JsonNode::Kind::string:  return Value{ heap.AllocString(node.string.data(), node.string.size()) };
		case JsonNode::Kind::array: {
			RootScope scope(vm.GetGC());
			const auto arr = scope.Keep(heap.AllocArray(node.items.size()));

			for (std::size_t i = 0; i < node.items.size(); i++) {
				// the element is the newest allocation: nothing can move it before it's stored
				const auto element = ToValue(vm, node.items[i]);
				arr->array.values[i] = element;
				vm.GetGC().WriteBarrier(arr.Obj(), element);
			}
			return arr.Get();
		}
		case JsonNode::Kind::object: {
			RootScope scope(vm.GetGC());
			const auto capacity = std::min<std::size_t>({ node.keys.size(), std::size_t{ 1024u }, static_cast<std::size_t>(std::numeric_limits<bloop::BloopIndex>::max()) });
			const auto obj = scope.Keep(heap.AllocObject(static_cast<bloop::BloopIndex>(capacity)));

			for (std::size_t i = 0; i < node.keys.size(); i++) {
				const auto& key = node.keys[i];
				RootScope memberScope(vm.GetGC());
				const auto value = memberScope.Keep(ToValue(vm, node.items[i]));
				auto* keyString = heap.AllocString(key.data(), key.size());
				heap.ObjectSet(obj.Obj(), Value{ keyString }, value.Get()); // roots everything if it has to grow
			}
			return obj.Get();
		}
		}
		return Value{};
	}

	// the value of json text, or undefined if the text isn't valid json
	Value Parse(VM& vm, StringArg text) {
		JsonNode root;
		{
			CJsonParser parser(text.View()); // the view is only used before anything is allocated
			if (!parser.Parse(root))
				return Value{};
		}
		return ToValue(vm, root);
	}
}

NativeDef bloop::standard::GetJsonDefinitions() {
	return NativeDef(BLOOPTEXT("json"), NativeObject{
		.m_oFields = {
			NativeField(BLOOPTEXT("stringify"), Bind<&Stringify>()),
			NativeField(BLOOPTEXT("parse"), Bind<&Parse>()),
		}
	});
}
