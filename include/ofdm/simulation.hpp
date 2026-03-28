#pragma once

#include "ofdm/types.hpp"

#include <cstdint>
#include <random>

namespace ofdm {

struct SimParams {
    int N = 64;
    int cp_len = 16;
    int pilot_spacing = 8;
    int num_frames = 100;
    int rayleigh_taps = 4;
    ModScheme scheme = ModScheme::QPSK;
    // 1 = AWGN only, 2 = Rayleigh (freq-selective per OFDM symbol) + AWGN, 3 = ideal (no noise).
    int channel = 1;
    int equalizer = 1; // 1 MMSE, 2 ZF
    double snr_db = 20.0;
    bool verbose = true;
    // 0 = use random_device to seed the RNG (non-reproducible).
    uint32_t seed = 0;
};

struct SimResult {
    double ber = 0.0;
    int errors = 0;
    int bits_compared = 0;
    int frames_processed = 0;
    // Configured SNR is treated as Es/N0 (avg symbol energy / noise) for Eb/N0 reporting.
    double es_n0_db = 0.0;
    double eb_n0_db = 0.0;
};

std::mt19937 make_rng(uint32_t seed);

SimResult run_ofdm_sim(const SimParams& p, std::mt19937& gen);

} // namespace ofdm
