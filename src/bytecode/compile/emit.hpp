#pragma once
#include "utils/defs.hpp"
#include "bytecode/defs.hpp"

#include <vector>
#include <ranges>
#include <optional>
#include <iostream>
#include <ranges>
#include <algorithm>

namespace bloop::bytecode
{
	struct CConstant {
		bloop::BloopString m_pConstant;
		bloop::EValueType m_eDataType{};
	};

	struct CSingularByteCode {		
		EOpCode m_eOpCode;
		std::optional<bloop::BloopInt> m_iIndex;

		auto ToString() const {
			bloop::BloopString a = stringConversionTable[m_eOpCode];

			if (m_iIndex)
				a += ", " + std::to_string(*m_iIndex);

			return a;

		}
	};

	struct CByteCodeBuilder {
		
		[[nodiscard]] bloop::BloopInt AddConstant(CConstant&& c) {

			if (const auto ptr = std::ranges::find_if(m_oConstants,
				[&c](const CConstant& p) {
					return p.m_eDataType == c.m_eDataType && p.m_pConstant == c.m_pConstant;
				}); ptr != m_oConstants.end()) {
				return std::distance(m_oConstants.begin(), ptr);
			}

			auto idx = m_oConstants.size();
			m_oConstants.emplace_back(std::forward<decltype(c)>(c));
			return static_cast<bloop::BloopInt>(idx);
		}
		void Emit(EOpCode opcode, bloop::BloopInt idx) {
			m_oByteCode.emplace_back(CSingularByteCode{ .m_eOpCode = opcode, .m_iIndex = idx });
		}
		void Emit(EOpCode opcode) {
			m_oByteCode.emplace_back(CSingularByteCode{ .m_eOpCode = opcode, .m_iIndex = std::nullopt });
		}

		void Print() {

			std::ranges::for_each(m_oByteCode, [](const CSingularByteCode& c) {
				std::cout << c.ToString() << '\n';
			});

		}

		std::vector<CConstant> m_oConstants;
		std::vector<CSingularByteCode> m_oByteCode;
	};

}
