#include "bytecode/compile/emit.hpp"
#include "bytecode/exception.hpp"

#include <vector>
#include <ranges>
#include <optional>
#include <iostream>
#include <ranges>
#include <algorithm>

using namespace bloop::bytecode;

bloop::BloopUInt16 CByteCodeBuilder::AddConstant(CConstant&& c) {

	if (const auto ptr = std::ranges::find_if(m_oConstants,
		[&c](const CConstant& p) {
			return p.m_eDataType == c.m_eDataType && p.m_pConstant == c.m_pConstant;
		}); ptr != m_oConstants.end()) {

		const auto dist = std::distance(m_oConstants.begin(), ptr);

		if (dist > std::numeric_limits<bloop::BloopUInt16>::max())
			throw exception::ByteCodeError(BLOOPTEXT("more than 65536 constants in a function"));

		return static_cast<bloop::BloopUInt16>(std::distance(m_oConstants.begin(), ptr));
	}

	auto idx = m_oConstants.size();
	m_oConstants.emplace_back(std::forward<decltype(c)>(c));
	return static_cast<bloop::BloopUInt16>(idx);
}
void CByteCodeBuilder::Emit(EOpCode opcode, bloop::BloopUInt16 idx, CodePosition pos) {
	m_oByteCode.emplace_back(CSingularByteCode{ .ins = Instr1{.op = opcode, .arg = idx }, .loc = { m_uOffset, pos } });
	m_uOffset += 3; // op = 1, idx = 2
}
void CByteCodeBuilder::Emit(EOpCode opcode, CodePosition pos) {
	m_oByteCode.emplace_back(CSingularByteCode{ .ins = Instr0{.op = opcode }, .loc = { m_uOffset, pos } });
	m_uOffset += 1; // op = 1
}

bloop::BloopUInt16 CByteCodeBuilder::EmitJump(EOpCode opcode, CodePosition pos) {
	auto idx = m_oByteCode.size();
	Emit(opcode, 0, pos);
	return static_cast<bloop::BloopUInt16>(idx);
}
void CByteCodeBuilder::EmitJump(EOpCode opcode, bloop::BloopUInt16 offset, CodePosition pos) {
	Emit(opcode, offset, pos);
}
void CByteCodeBuilder::PatchJump(bloop::BloopUInt16 src, bloop::BloopUInt16 dst) {
	std::get<1>(m_oByteCode[src].ins).arg = dst;
}

void CByteCodeBuilder::Print() {

	bloop::BloopUInt16 ip{};

	std::ranges::for_each(m_oByteCode, [&ip](const CSingularByteCode& c) {
		std::cout << ip << ": " << c.ToString() << '\n';
		ip += c.GetBytes();
	});

}
std::vector<bloop::BloopByte> CByteCodeBuilder::Encode() {
	std::vector<bloop::BloopByte> out;

	for (auto& bc : m_oByteCode) {
		std::visit([&](auto&& i) {
			out.push_back(static_cast<bloop::BloopByte>(i.op));

			if constexpr (std::is_same_v<std::decay_t<decltype(i)>, Instr1>) {
				out.push_back(i.arg & 0xFF);
				out.push_back((i.arg >> 8) & 0xFF);
			}
		}, bc.ins);
	}
	return out;
}

std::vector<CInstructionPosition> CByteCodeBuilder::GetCodePositions()
{
	std::vector<CInstructionPosition> locs;
	locs.reserve(m_oByteCode.size());
	for (auto& bc : m_oByteCode)
		locs.push_back(bc.loc);

	return locs;
}