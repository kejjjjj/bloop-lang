#pragma once

#include "utils/defs.hpp"
#include "bytecode/bc_common.hpp"

#include <vector>
#include <unordered_map>

namespace bloop::metadata {

	struct VMMetadata {

		[[maybe_unused]] bloop::BloopIndex AddChunk(const bloop::bc::Chunk& data) {
			const auto idx = static_cast<bloop::BloopIndex>(m_oChunks.size());
			m_oChunks.emplace_back(data).m_uId = idx;
			return idx;
		}
		[[nodiscard]] bloop::BloopIndex AddFunction(const bloop::bc::Function& data) {
			const auto idx = static_cast<bloop::BloopIndex>(m_oFunctions.size());
			m_oFunctions.emplace_back(data).m_uId = idx;
			return idx;
		}

		std::vector<bloop::bc::Chunk> m_oChunks;
		std::vector<bloop::bc::Function> m_oFunctions; // references m_oFunctionDebugInfo
		bloop::bc::Chunk* m_pGlobalChunk{};
	};

	struct Metadata {

		//contains every line of the source code
		//VMMetadata references this in debug builds
		std::vector<bloop::BloopStringView> m_oLineMap; 

		struct FunctionDebugInfo {

			//references m_oLineMap
			bloop::BloopIndex m_uId;
			bloop::BloopString m_sName;
			bloop::BloopUInt m_uStartLine;
			bloop::BloopUInt m_uEndLine;

		};

		std::vector<FunctionDebugInfo> m_oFunctionDebugInfo;
		std::unordered_map<bloop::BloopString, FunctionDebugInfo*> m_oFunctionTable;
		bloop::BloopIndex m_uNumGlobals{};

		VMMetadata m_oVMData;
	};

}