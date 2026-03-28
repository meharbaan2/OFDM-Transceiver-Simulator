#include "ofdm/self_test.hpp"

#include "ofdm/channel_awgn.hpp"
#include "ofdm/modulation.hpp"
#include "ofdm/ofdm_engine.hpp"
#include "ofdm/ofdm_frame.hpp"
#include "ofdm/pilots.hpp"
#include "ofdm/simulation.hpp"

#include <cmath>
#include <iostream>
#include <random>
#include <string>

namespace ofdm {

namespace {

int fail(const std::string& name, const std::string& detail) {
    std::cerr << "[FAIL] " << name << ": " << detail << "\n";
    return 1;
}

} // namespace

int run_self_tests() {
    int failures = 0;
    const double tol = 1e-9;

    // 1) FFT / IFFT round-trip (unitary scaling)
    {
        std::mt19937 gen(42u);
        std::normal_distribution<double> g(0.0, 1.0);
        OfdmEngine eng(64);
        std::vector<std::complex<double>> x(64), t, y;
        for (size_t i = 0; i < x.size(); i++)
            x[i] = std::complex<double>(g(gen), g(gen));
        eng.ifft(x, t);
        eng.fft(t, y);
        double err = 0.0;
        for (size_t i = 0; i < x.size(); i++)
            err = std::max(err, std::abs(x[i] - y[i]));
        if (err > 1e-10)
            failures += fail("fft_roundtrip", "max error " + std::to_string(err));
        else
            std::cerr << "[ OK ] fft_roundtrip\n";
    }

    // 2) Modulate / demodulate identity (no channel)
    {
        std::mt19937 gen(99u);
        std::vector<int> bits = generate_bits(1200, gen);
        for (ModScheme s : {ModScheme::QPSK, ModScheme::QAM16, ModScheme::QAM64}) {
            auto syms = modulate(bits, s);
            auto out = demodulate(syms, s);
            size_t n = std::min(bits.size(), out.size());
            bool ok = true;
            for (size_t i = 0; i < n; i++) {
                if (bits[i] != out[i]) {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                failures += fail("mod_demod", std::string("scheme ") + scheme_name(s));
            else
                std::cerr << "[ OK ] mod_demod " << scheme_name(s) << "\n";
        }
    }

    // 3) Pilot LS on ideal channel: H_hat ≈ 1
    {
        const int N = 64;
        const int pilot_spacing = 8;
        OfdmEngine eng(N);
        std::vector<std::complex<double>> frame(static_cast<size_t>(N));
        auto tx_pilots = pilot_sequence_alternating((N + pilot_spacing - 1) / pilot_spacing);
        for (int i = 0; i < N; i++) {
            if (i % pilot_spacing == 0) {
                int pi = i / pilot_spacing;
                frame[static_cast<size_t>(i)] = tx_pilots[static_cast<size_t>(pi) % tx_pilots.size()];
            } else
                frame[static_cast<size_t>(i)] = std::complex<double>(0.3, -0.4);
        }
        auto Hhat = estimate_channel_ls_interpolate(frame, tx_pilots, N, pilot_spacing);
        double max_e = 0.0;
        for (int k = 0; k < N; k++)
            max_e = std::max(max_e, std::abs(Hhat[static_cast<size_t>(k)] - 1.0));
        if (max_e > tol)
            failures += fail("pilot_ls_ideal", "max |H-1| = " + std::to_string(max_e));
        else
            std::cerr << "[ OK ] pilot_ls_ideal\n";
    }

    // 4) End-to-end ideal channel: BER == 0
    {
        SimParams p;
        p.channel = 3;
        p.scheme = ModScheme::QPSK;
        p.equalizer = 1;
        p.snr_db = 20.0;
        p.num_frames = 50;
        p.verbose = false;
        std::mt19937 gen(7u);
        SimResult r = run_ofdm_sim(p, gen);
        if (r.errors != 0 || r.ber != 0.0)
            failures += fail("e2e_ideal", "BER " + std::to_string(r.ber));
        else
            std::cerr << "[ OK ] e2e_ideal QPSK\n";
    }

    // 5) AWGN very high SNR: BER == 0 (single realization, QPSK)
    {
        SimParams p;
        p.channel = 1;
        p.scheme = ModScheme::QPSK;
        p.equalizer = 1;
        p.snr_db = 45.0;
        p.num_frames = 80;
        p.verbose = false;
        std::mt19937 gen(123u);
        SimResult r = run_ofdm_sim(p, gen);
        if (r.errors != 0)
            failures += fail("e2e_awgn_high_snr", "errors " + std::to_string(r.errors));
        else
            std::cerr << "[ OK ] e2e_awgn_high_snr QPSK\n";
    }

    // 6) OFDM mod/demod identity without fading (freq domain impulse check via single tone)
    {
        const int N = 32;
        const int cp = 8;
        OfdmEngine eng(N);
        std::vector<std::complex<double>> X(static_cast<size_t>(N), 0.0);
        X[3] = std::complex<double>(1.0, -0.5);
        std::vector<std::complex<double>> tx = ofdm_mod(X, eng, cp);
        std::vector<std::complex<double>> Y = ofdm_demod(tx, eng, cp);
        double err = 0.0;
        for (int k = 0; k < N; k++)
            err = std::max(err, std::abs(X[static_cast<size_t>(k)] - Y[static_cast<size_t>(k)]));
        if (err > 1e-10)
            failures += fail("ofdm_single_tone", "max error " + std::to_string(err));
        else
            std::cerr << "[ OK ] ofdm_single_tone\n";
    }

    // 7) Rayleigh + moderate SNR: BER finite, no crash, bits compared full
    {
        SimParams p;
        p.channel = 2;
        p.rayleigh_taps = 4;
        p.cp_len = 16;
        p.scheme = ModScheme::QPSK;
        p.equalizer = 1;
        p.snr_db = 18.0;
        p.num_frames = 30;
        p.verbose = false;
        std::mt19937 gen(999u);
        SimResult r = run_ofdm_sim(p, gen);
        if (r.bits_compared <= 0 || r.frames_processed <= 0)
            failures += fail("e2e_rayleigh_smoke", "no bits compared");
        else
            std::cerr << "[ OK ] e2e_rayleigh_smoke BER=" << r.ber << "\n";
    }

    if (failures == 0)
        std::cerr << "\nAll self-tests passed.\n";
    else
        std::cerr << "\n" << failures << " test(s) failed.\n";

    return failures;
}

} // namespace ofdm
