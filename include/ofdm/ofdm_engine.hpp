#pragma once

#include <complex>
#include <memory>
#include <vector>

namespace ofdm {

// Unitary-style OFDM DFT: forward and inverse each apply 1/sqrt(N) scaling.
class OfdmEngine {
public:
    explicit OfdmEngine(int n_fft);
    ~OfdmEngine();

    OfdmEngine(const OfdmEngine&) = delete;
    OfdmEngine& operator=(const OfdmEngine&) = delete;

    int n() const;

    void ifft(const std::vector<std::complex<double>>& freq, std::vector<std::complex<double>>& time_out);
    void fft(const std::vector<std::complex<double>>& time_in, std::vector<std::complex<double>>& freq_out);

    std::vector<std::complex<double>> freq_response_from_taps(const std::vector<std::complex<double>>& h_taps);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ofdm
