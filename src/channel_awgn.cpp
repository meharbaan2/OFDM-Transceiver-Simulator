#include "ofdm/channel_awgn.hpp"

#include <cmath>
#include <random>

namespace ofdm {

std::vector<std::complex<double>> awgn(
    const std::vector<std::complex<double>>& signal, double snr_db, std::mt19937& gen) {
    if (signal.empty()) return {};

    double sig_power = 0.0;
    for (const auto& s : signal) sig_power += std::norm(s);
    sig_power /= static_cast<double>(signal.size());

    const double snr_lin = std::pow(10.0, snr_db / 10.0);
    const double noise_var = sig_power / snr_lin;
    const double sigma = std::sqrt(noise_var / 2.0);

    std::normal_distribution<double> dist(0.0, sigma);
    std::vector<std::complex<double>> noisy(signal.size());
    for (size_t i = 0; i < signal.size(); i++)
        noisy[i] = signal[i] + std::complex<double>(dist(gen), dist(gen));
    return noisy;
}

} // namespace ofdm
