#pragma once

#include <complex>
#include <vector>

namespace ofdm {

// Scalar per-subcarrier MMSE: W = H* / (|H|^2 + sigma^2), sigma^2 = 10^(-SNR_dB/10), Es ~ 1.
std::vector<std::complex<double>> equalize_mmse(const std::vector<std::complex<double>>& rx,
    const std::vector<std::complex<double>>& H, double snr_db);

// Regularized ZF: Y * H* / max(|H|^2, eps^2).
std::vector<std::complex<double>> equalize_zf(const std::vector<std::complex<double>>& rx,
    const std::vector<std::complex<double>>& H, double eps = 1e-6);

} // namespace ofdm
