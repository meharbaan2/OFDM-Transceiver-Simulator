#include "ofdm/ofdm_frame.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace ofdm {

std::vector<std::complex<double>> ofdm_mod(
    const std::vector<std::complex<double>>& symbols, OfdmEngine& eng, int cp_len) {
    const int N = eng.n();
    const int num_symbols = static_cast<int>(symbols.size());
    const int num_frames = (num_symbols + N - 1) / N;

    std::vector<std::complex<double>> tx;
    tx.reserve(static_cast<size_t>(num_frames) * static_cast<size_t>(N + cp_len));

    std::vector<std::complex<double>> frame(static_cast<size_t>(N)), time_block;
    for (int f = 0; f < num_frames; f++) {
        std::fill(frame.begin(), frame.end(), std::complex<double>(0.0, 0.0));
        const int base = f * N;
        const int copy = std::min(N, num_symbols - base);
        for (int k = 0; k < copy; k++) frame[static_cast<size_t>(k)] = symbols[static_cast<size_t>(base + k)];

        eng.ifft(frame, time_block);

        for (int k = N - cp_len; k < N; k++)
            tx.push_back(time_block[static_cast<size_t>(k)]);
        for (int k = 0; k < N; k++)
            tx.push_back(time_block[static_cast<size_t>(k)]);
    }
    return tx;
}

std::vector<std::complex<double>> ofdm_demod(
    const std::vector<std::complex<double>>& rx, OfdmEngine& eng, int cp_len) {
    const int N = eng.n();
    const int frame_len = N + cp_len;
    if (rx.size() < static_cast<size_t>(frame_len)) return {};

    const int total_frames = static_cast<int>(rx.size() / static_cast<size_t>(frame_len));
    std::vector<std::complex<double>> symbols;
    symbols.reserve(static_cast<size_t>(total_frames) * static_cast<size_t>(N));

    std::vector<std::complex<double>> time_block(static_cast<size_t>(N)), freq_block;
    for (int f = 0; f < total_frames; f++) {
        const size_t start = static_cast<size_t>(f) * static_cast<size_t>(frame_len);
        if (start + static_cast<size_t>(frame_len) > rx.size()) break;
        for (int k = 0; k < N; k++)
            time_block[static_cast<size_t>(k)] = rx[start + static_cast<size_t>(cp_len + k)];
        eng.fft(time_block, freq_block);
        symbols.insert(symbols.end(), freq_block.begin(), freq_block.end());
    }
    return symbols;
}

void apply_rayleigh_fading_frequency(std::vector<std::complex<double>>& freq_flat, int N, int num_taps,
    OfdmEngine& eng, std::mt19937& gen) {
    if (freq_flat.empty() || N <= 0) return;
    std::normal_distribution<double> gauss(0.0, 1.0);
    const int frames = static_cast<int>(freq_flat.size() / static_cast<size_t>(N));
    std::vector<std::complex<double>> h(static_cast<size_t>(std::max(1, num_taps)));

    for (int f = 0; f < frames; f++) {
        double p = 0.0;
        for (size_t i = 0; i < h.size(); i++) {
            h[i] = std::complex<double>(gauss(gen), gauss(gen)) / std::sqrt(2.0);
            p += std::norm(h[i]);
        }
        const double scale = 1.0 / std::sqrt(p);
        for (auto& x : h) x *= scale;

        std::vector<std::complex<double>> H = eng.freq_response_from_taps(h);
        for (int k = 0; k < N; k++)
            freq_flat[static_cast<size_t>(f) * static_cast<size_t>(N) + static_cast<size_t>(k)] *= H[static_cast<size_t>(k)];
    }
}

} // namespace ofdm
