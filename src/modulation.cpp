#include "ofdm/modulation.hpp"

#include <algorithm>
#include <cmath>

namespace ofdm {

int bits_per_symbol(ModScheme s) {
    switch (s) {
    case ModScheme::QPSK: return 2;
    case ModScheme::QAM16: return 4;
    case ModScheme::QAM64: return 6;
    }
    return 2;
}

const char* scheme_name(ModScheme s) {
    switch (s) {
    case ModScheme::QPSK: return "QPSK";
    case ModScheme::QAM16: return "16-QAM";
    case ModScheme::QAM64: return "64-QAM";
    }
    return "?";
}

std::vector<int> generate_bits(int n, std::mt19937& gen) {
    std::uniform_int_distribution<> dis(0, 1);
    std::vector<int> bits(static_cast<size_t>(n));
    for (int i = 0; i < n; i++) bits[static_cast<size_t>(i)] = dis(gen);
    return bits;
}

std::vector<std::complex<double>> modulate(const std::vector<int>& bits, ModScheme scheme) {
    std::vector<std::complex<double>> symbols;

    if (scheme == ModScheme::QPSK) {
        for (size_t i = 0; i + 1 < bits.size(); i += 2) {
            double re = (bits[i] == 0) ? -1.0 : 1.0;
            double im = (bits[i + 1] == 0) ? -1.0 : 1.0;
            symbols.emplace_back(re / std::sqrt(2.0), im / std::sqrt(2.0));
        }
    } else if (scheme == ModScheme::QAM16) {
        const double norm = 1.0 / std::sqrt(10.0);
        for (size_t i = 0; i + 3 < bits.size(); i += 4) {
            int b0 = bits[i], b1 = bits[i + 1], b2 = bits[i + 2], b3 = bits[i + 3];
            int re_level = (2 * b0 + b1) * 2 - 3;
            int im_level = (2 * b2 + b3) * 2 - 3;
            symbols.emplace_back(re_level * norm, im_level * norm);
        }
    } else if (scheme == ModScheme::QAM64) {
        const double norm = 1.0 / std::sqrt(42.0);
        for (size_t i = 0; i + 5 < bits.size(); i += 6) {
            int b0 = bits[i], b1 = bits[i + 1], b2 = bits[i + 2];
            int b3 = bits[i + 3], b4 = bits[i + 4], b5 = bits[i + 5];
            int re_level = (4 * b0 + 2 * b1 + b2) * 2 - 7;
            int im_level = (4 * b3 + 2 * b4 + b5) * 2 - 7;
            symbols.emplace_back(re_level * norm, im_level * norm);
        }
    }

    return symbols;
}

std::vector<int> demodulate(const std::vector<std::complex<double>>& symbols, ModScheme scheme) {
    std::vector<int> bits;

    if (scheme == ModScheme::QPSK) {
        for (const auto& s : symbols) {
            bits.push_back(std::real(s) > 0 ? 1 : 0);
            bits.push_back(std::imag(s) > 0 ? 1 : 0);
        }
    } else if (scheme == ModScheme::QAM16) {
        const double norm = 1.0 / std::sqrt(10.0);
        for (const auto& s : symbols) {
            double re = std::real(s) / norm;
            double im = std::imag(s) / norm;
            int re_lev = static_cast<int>(std::lround((re + 3) / 2));
            int im_lev = static_cast<int>(std::lround((im + 3) / 2));
            re_lev = std::max(0, std::min(3, re_lev));
            im_lev = std::max(0, std::min(3, im_lev));
            bits.push_back((re_lev >> 1) & 1);
            bits.push_back(re_lev & 1);
            bits.push_back((im_lev >> 1) & 1);
            bits.push_back(im_lev & 1);
        }
    } else if (scheme == ModScheme::QAM64) {
        const double norm = 1.0 / std::sqrt(42.0);
        for (const auto& s : symbols) {
            double re = std::real(s) / norm;
            double im = std::imag(s) / norm;
            int re_lev = static_cast<int>(std::lround((re + 7) / 2));
            int im_lev = static_cast<int>(std::lround((im + 7) / 2));
            re_lev = std::max(0, std::min(7, re_lev));
            im_lev = std::max(0, std::min(7, im_lev));
            bits.push_back((re_lev >> 2) & 1);
            bits.push_back((re_lev >> 1) & 1);
            bits.push_back(re_lev & 1);
            bits.push_back((im_lev >> 2) & 1);
            bits.push_back((im_lev >> 1) & 1);
            bits.push_back(im_lev & 1);
        }
    }

    return bits;
}

double eb_n0_db(double es_n0_db, ModScheme scheme) {
    const int b = bits_per_symbol(scheme);
    if (b <= 0) return es_n0_db;
    return es_n0_db - 10.0 * std::log10(static_cast<double>(b));
}

} // namespace ofdm
