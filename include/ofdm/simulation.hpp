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
    uint32_t seed = 0; // unused; RNG is passed into run_ofdm_sim
};

struct SimResult {
    double ber = 0.0;
    int errors = 0;
    int bits_compared = 0;
    int frames_processed = 0;
};

SimResult run_ofdm_sim(const SimParams& p, std::mt19937& gen);

} // namespace ofdm
