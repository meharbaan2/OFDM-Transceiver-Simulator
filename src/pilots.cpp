#include "ofdm/pilots.hpp"

#include <algorithm>

namespace ofdm {

std::vector<std::complex<double>> pilot_sequence_alternating(int pilots_per_frame) {
    std::vector<std::complex<double>> seq(static_cast<size_t>(pilots_per_frame));
    for (int i = 0; i < pilots_per_frame; i++)
        seq[static_cast<size_t>(i)] =
            (i % 2 == 0) ? std::complex<double>(1.0, 0.0) : std::complex<double>(-1.0, 0.0);
    return seq;
}

std::vector<std::complex<double>> insert_pilots(const std::vector<std::complex<double>>& data_symbols,
    int N, int pilot_spacing, int num_frames) {
    std::vector<std::complex<double>> out;
    const int pilots_per_frame = (N + pilot_spacing - 1) / pilot_spacing;
    const int data_per_frame = N - pilots_per_frame;
    const auto pilots = pilot_sequence_alternating(pilots_per_frame);

    out.reserve(static_cast<size_t>(num_frames) * static_cast<size_t>(N));
    for (int frame = 0; frame < num_frames; frame++) {
        for (int i = 0; i < N; i++) {
            if (i % pilot_spacing == 0) {
                const int pi = i / pilot_spacing;
                out.push_back(pilots[static_cast<size_t>(pi) % pilots.size()]);
            } else {
                const int data_idx = frame * data_per_frame + (i - (i / pilot_spacing) - 1);
                if (data_idx >= 0 && data_idx < static_cast<int>(data_symbols.size()))
                    out.push_back(data_symbols[static_cast<size_t>(data_idx)]);
                else
                    out.push_back(0.0);
            }
        }
    }
    return out;
}

std::vector<std::complex<double>> extract_pilots(
    const std::vector<std::complex<double>>& frame_symbols, int N, int pilot_spacing) {
    std::vector<std::complex<double>> pilots;
    for (int i = 0; i < N; i += pilot_spacing)
        pilots.push_back(frame_symbols[static_cast<size_t>(i)]);
    return pilots;
}

std::vector<std::complex<double>> extract_data(
    const std::vector<std::complex<double>>& symbols_with_pilots, int N, int pilot_spacing) {
    std::vector<std::complex<double>> data;
    const int num_frames = static_cast<int>(symbols_with_pilots.size() / static_cast<size_t>(N));
    for (int frame = 0; frame < num_frames; frame++) {
        for (int i = 0; i < N; i++) {
            if (i % pilot_spacing == 0) continue;
            const int idx = frame * N + i;
            if (idx < static_cast<int>(symbols_with_pilots.size()))
                data.push_back(symbols_with_pilots[static_cast<size_t>(idx)]);
        }
    }
    return data;
}

std::vector<std::complex<double>> estimate_channel_ls_interpolate(
    const std::vector<std::complex<double>>& rx_frame, const std::vector<std::complex<double>>& tx_pilots,
    int N, int pilot_spacing) {
    std::vector<std::complex<double>> H(static_cast<size_t>(N), 1.0);
    const int P = (N + pilot_spacing - 1) / pilot_spacing;
    std::vector<std::complex<double>> Hp(static_cast<size_t>(P), 1.0);

    for (int p = 0; p < P; p++) {
        const int k = p * pilot_spacing;
        const std::complex<double> xp = tx_pilots[static_cast<size_t>(p) % tx_pilots.size()];
        const std::complex<double> yp = rx_frame[static_cast<size_t>(k)];
        if (std::abs(xp) > 1e-9)
            Hp[static_cast<size_t>(p)] = yp / xp;
        else
            Hp[static_cast<size_t>(p)] = 1.0;
    }

    for (int k = 0; k < N; k++) {
        const int left_p = k / pilot_spacing;
        int right_p = left_p + 1;
        const int k_left = left_p * pilot_spacing;
        int k_right = right_p * pilot_spacing;
        if (k_right >= N) {
            right_p = left_p;
            k_right = std::min(N - 1, k_left + pilot_spacing);
        }
        if (k_right == k_left) {
            H[static_cast<size_t>(k)] = Hp[static_cast<size_t>(left_p)];
            continue;
        }
        const double t = static_cast<double>(k - k_left) / static_cast<double>(k_right - k_left);
        H[static_cast<size_t>(k)] =
            (1.0 - t) * Hp[static_cast<size_t>(left_p)] + t * Hp[static_cast<size_t>(right_p)];
    }
    return H;
}

} // namespace ofdm
