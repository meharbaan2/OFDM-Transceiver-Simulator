#include "ofdm/equalizer.hpp"

#include <algorithm>
#include <cmath>

namespace ofdm {

std::vector<std::complex<double>> equalize_mmse(const std::vector<std::complex<double>>& rx,
    const std::vector<std::complex<double>>& H, double snr_db) {
    const double sigma2 = std::pow(10.0, -snr_db / 10.0);
    std::vector<std::complex<double>> z(rx.size());
    for (size_t i = 0; i < rx.size(); i++) {
        const std::complex<double> h = H[i % H.size()];
        const double denom = std::norm(h) + sigma2 + 1e-20;
        z[i] = rx[i] * std::conj(h) / denom;
    }
    return z;
}

std::vector<std::complex<double>> equalize_zf(const std::vector<std::complex<double>>& rx,
    const std::vector<std::complex<double>>& H, double eps) {
    std::vector<std::complex<double>> z(rx.size());
    for (size_t i = 0; i < rx.size(); i++) {
        const std::complex<double> h = H[i % H.size()];
        const double nh = std::norm(h);
        const double denom = std::max(nh, eps * eps);
        z[i] = rx[i] * std::conj(h) / denom;
    }
    return z;
}

} // namespace ofdm
