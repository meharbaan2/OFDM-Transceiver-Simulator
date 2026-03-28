#include "ofdm/simulation.hpp"

#include "ofdm/channel_awgn.hpp"
#include "ofdm/equalizer.hpp"
#include "ofdm/modulation.hpp"
#include "ofdm/ofdm_engine.hpp"
#include "ofdm/ofdm_frame.hpp"
#include "ofdm/pilots.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace ofdm {

std::mt19937 make_rng(uint32_t seed) {
    if (seed == 0) {
        std::random_device rd;
        return std::mt19937(rd());
    }
    return std::mt19937(seed);
}

SimResult run_ofdm_sim(const SimParams& p, std::mt19937& gen) {
    SimResult r;
    OfdmEngine eng(p.N);

    if (p.cp_len < p.rayleigh_taps && p.channel == 2 && p.verbose) {
        std::cerr << "Note: cp_len < rayleigh_taps; CP should cover delay spread (L-1) for the circ. conv. model.\n";
    }

    const int pilots_per_frame = (p.N + p.pilot_spacing - 1) / p.pilot_spacing;
    const int data_per_frame = p.N - pilots_per_frame;
    const int bps = bits_per_symbol(p.scheme);
    const int num_data_syms = p.num_frames * data_per_frame;
    const int num_bits = num_data_syms * bps;

    std::vector<int> bits = generate_bits(num_bits, gen);
    std::vector<std::complex<double>> data_syms = modulate(bits, p.scheme);

    std::vector<std::complex<double>> freq_syms = insert_pilots(data_syms, p.N, p.pilot_spacing, p.num_frames);
    const auto tx_pilots = pilot_sequence_alternating(pilots_per_frame);

    if (p.channel == 2)
        apply_rayleigh_fading_frequency(freq_syms, p.N, p.rayleigh_taps, eng, gen);

    std::vector<std::complex<double>> tx_time = ofdm_mod(freq_syms, eng, p.cp_len);

    std::vector<std::complex<double>> rx_time;
    if (p.channel == 1 || p.channel == 2)
        rx_time = awgn(tx_time, p.snr_db, gen);
    else
        rx_time = tx_time;

    std::vector<std::complex<double>> rx_freq = ofdm_demod(rx_time, eng, p.cp_len);

    r.frames_processed = std::min(p.num_frames, static_cast<int>(rx_freq.size() / static_cast<size_t>(p.N)));
    std::vector<std::complex<double>> all_data;

    for (int frame = 0; frame < r.frames_processed; frame++) {
        std::vector<std::complex<double>> frame_sym(static_cast<size_t>(p.N));
        for (int i = 0; i < p.N; i++)
            frame_sym[static_cast<size_t>(i)] =
                rx_freq[static_cast<size_t>(frame) * static_cast<size_t>(p.N) + static_cast<size_t>(i)];

        std::vector<std::complex<double>> H =
            estimate_channel_ls_interpolate(frame_sym, tx_pilots, p.N, p.pilot_spacing);

        std::vector<std::complex<double>> eq =
            (p.equalizer == 2) ? equalize_zf(frame_sym, H) : equalize_mmse(frame_sym, H, p.snr_db);

        std::vector<std::complex<double>> fd = extract_data(eq, p.N, p.pilot_spacing);
        all_data.insert(all_data.end(), fd.begin(), fd.end());
    }

    std::vector<int> rx_bits = demodulate(all_data, p.scheme);
    const int ncmp = std::min(static_cast<int>(bits.size()), static_cast<int>(rx_bits.size()));
    for (int i = 0; i < ncmp; i++)
        if (bits[static_cast<size_t>(i)] != rx_bits[static_cast<size_t>(i)]) r.errors++;
    r.bits_compared = ncmp;
    r.ber = ncmp > 0 ? static_cast<double>(r.errors) / static_cast<double>(ncmp) : 0.0;
    r.es_n0_db = p.snr_db;
    r.eb_n0_db = eb_n0_db(p.snr_db, p.scheme);
    return r;
}

} // namespace ofdm
