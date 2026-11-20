#pragma once

#include <cstdint>

namespace bloop {

	// xoshiro256** (Blackman & Vigna): small, fast and good enough for games. The same seed gives the same
	// numbers on every platform, so a game can replay them (std.math.random, std.math.seed)
	class Random {
	public:
		explicit Random(std::uint64_t seed = 0u) noexcept { Seed(seed); }

		void Seed(std::uint64_t seed) noexcept {
			// splitmix64 spreads the seed over the state (which must not be all zeros)
			for (auto& word : m_uState) {
				seed += 0x9E3779B97F4A7C15ull;
				std::uint64_t z = seed;
				z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
				z = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
				word = z ^ (z >> 31u);
			}
		}

		[[nodiscard]] std::uint64_t Next() noexcept {
			const auto result = Rotl(m_uState[1] * 5u, 7) * 9u;
			const auto t = m_uState[1] << 17u;

			m_uState[2] ^= m_uState[0];
			m_uState[3] ^= m_uState[1];
			m_uState[1] ^= m_uState[2];
			m_uState[0] ^= m_uState[3];
			m_uState[2] ^= t;
			m_uState[3] = Rotl(m_uState[3], 45);
			return result;
		}

		// [0, 1), with all 53 bits of a double
		[[nodiscard]] double NextDouble() noexcept {
			return static_cast<double>(Next() >> 11u) * 0x1.0p-53;
		}

		// [0, bound) without bias (bound > 0)
		[[nodiscard]] std::uint64_t Below(std::uint64_t bound) noexcept {
			// throw away the top values that would make some results more likely than others
			const auto limit = (0u - bound) % bound; // == 2^64 % bound
			for (;;) {
				const auto r = Next();
				if (r >= limit)
					return r % bound;
			}
		}

	private:
		[[nodiscard]] static constexpr std::uint64_t Rotl(std::uint64_t x, int k) noexcept {
			return (x << k) | (x >> (64 - k));
		}

		std::uint64_t m_uState[4]{};
	};
}
