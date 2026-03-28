#pragma once

#include "ofdm/types.hpp"

#include <complex>
#include <random>
#include <vector>

namespace ofdm {

std::vector<int> generate_bits(int n, std::mt19937& gen);

std::vector<std::complex<double>> modulate(const std::vector<int>& bits, ModScheme scheme);

std::vector<int> demodulate(const std::vector<std::complex<double>>& symbols, ModScheme scheme);

// Eb/N0 (dB) from Es/N0 (dB), assuming Gray mapping: Eb/N0 = Es/N0 / bps.
double eb_n0_db(double es_n0_db, ModScheme scheme);

} // namespace ofdm
