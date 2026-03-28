#pragma once

#include "ofdm/ofdm_engine.hpp"

#include <complex>
#include <random>
#include <vector>

namespace ofdm {

std::vector<std::complex<double>> ofdm_mod(
    const std::vector<std::complex<double>>& freq_symbols, OfdmEngine& eng, int cp_len);

std::vector<std::complex<double>> ofdm_demod(
    const std::vector<std::complex<double>>& rx_time, OfdmEngine& eng, int cp_len);

// Per-OFDM-symbol frequency-selective Rayleigh: Y[k] = H[k]X[k], H = FFT of zero-padded CIR (unit energy).
void apply_rayleigh_fading_frequency(std::vector<std::complex<double>>& freq_flat, int N, int num_taps,
    OfdmEngine& eng, std::mt19937& gen);

} // namespace ofdm
