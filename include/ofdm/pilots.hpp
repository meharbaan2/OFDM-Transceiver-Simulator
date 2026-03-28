#pragma once

#include <complex>
#include <vector>

namespace ofdm {

std::vector<std::complex<double>> pilot_sequence_alternating(int pilots_per_frame);

std::vector<std::complex<double>> insert_pilots(const std::vector<std::complex<double>>& data_symbols,
    int N, int pilot_spacing, int num_frames);

std::vector<std::complex<double>> extract_pilots(
    const std::vector<std::complex<double>>& frame_symbols, int N, int pilot_spacing);

std::vector<std::complex<double>> extract_data(
    const std::vector<std::complex<double>>& symbols_with_pilots, int N, int pilot_spacing);

// LS at pilot subcarriers, linear interpolation of complex H between pilots.
std::vector<std::complex<double>> estimate_channel_ls_interpolate(
    const std::vector<std::complex<double>>& rx_frame, const std::vector<std::complex<double>>& tx_pilots,
    int N, int pilot_spacing);

} // namespace ofdm
