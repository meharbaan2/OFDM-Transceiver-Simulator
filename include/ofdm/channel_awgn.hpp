#pragma once

#include <complex>
#include <random>
#include <vector>

namespace ofdm {

// SNR (dB) = 10*log10( E[|s|^2] / sigma_n^2 ) with complex AWGN variance sigma_n^2 on |.|^2.
std::vector<std::complex<double>> awgn(
    const std::vector<std::complex<double>>& signal, double snr_db, std::mt19937& gen);

} // namespace ofdm
