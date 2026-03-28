#include "ofdm/ofdm_engine.hpp"

#include <cmath>
#include <fftw3.h>

namespace ofdm {

struct OfdmEngine::Impl {
    int N = 0;
    fftw_complex* in = nullptr;
    fftw_complex* out = nullptr;
    fftw_plan plan_fwd = nullptr;
    fftw_plan plan_rev = nullptr;
};

OfdmEngine::OfdmEngine(int n_fft) : impl_(std::make_unique<Impl>()) {
    impl_->N = n_fft;
    impl_->in = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * static_cast<size_t>(n_fft)));
    impl_->out = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * static_cast<size_t>(n_fft)));
    impl_->plan_fwd = fftw_plan_dft_1d(n_fft, impl_->in, impl_->out, FFTW_FORWARD, FFTW_ESTIMATE);
    impl_->plan_rev = fftw_plan_dft_1d(n_fft, impl_->in, impl_->out, FFTW_BACKWARD, FFTW_ESTIMATE);
}

OfdmEngine::~OfdmEngine() {
    if (impl_) {
        fftw_destroy_plan(impl_->plan_rev);
        fftw_destroy_plan(impl_->plan_fwd);
        fftw_free(impl_->out);
        fftw_free(impl_->in);
    }
}

int OfdmEngine::n() const { return impl_->N; }

void OfdmEngine::ifft(const std::vector<std::complex<double>>& freq, std::vector<std::complex<double>>& time_out) {
    const int N = impl_->N;
    time_out.resize(static_cast<size_t>(N));
    for (int k = 0; k < N; k++) {
        impl_->in[k][0] = std::real(freq[static_cast<size_t>(k)]);
        impl_->in[k][1] = std::imag(freq[static_cast<size_t>(k)]);
    }
    fftw_execute(impl_->plan_rev);
    const double scale = 1.0 / std::sqrt(static_cast<double>(N));
    for (int k = 0; k < N; k++)
        time_out[static_cast<size_t>(k)] =
            std::complex<double>(impl_->out[k][0] * scale, impl_->out[k][1] * scale);
}

void OfdmEngine::fft(const std::vector<std::complex<double>>& time_in, std::vector<std::complex<double>>& freq_out) {
    const int N = impl_->N;
    freq_out.resize(static_cast<size_t>(N));
    for (int k = 0; k < N; k++) {
        impl_->in[k][0] = std::real(time_in[static_cast<size_t>(k)]);
        impl_->in[k][1] = std::imag(time_in[static_cast<size_t>(k)]);
    }
    fftw_execute(impl_->plan_fwd);
    const double scale = 1.0 / std::sqrt(static_cast<double>(N));
    for (int k = 0; k < N; k++)
        freq_out[static_cast<size_t>(k)] =
            std::complex<double>(impl_->out[k][0] * scale, impl_->out[k][1] * scale);
}

std::vector<std::complex<double>> OfdmEngine::freq_response_from_taps(const std::vector<std::complex<double>>& h_taps) {
    const int N = impl_->N;
    std::vector<std::complex<double>> padded(static_cast<size_t>(N), 0.0);
    const int L = std::min(N, static_cast<int>(h_taps.size()));
    for (int i = 0; i < L; i++) padded[static_cast<size_t>(i)] = h_taps[static_cast<size_t>(i)];
    std::vector<std::complex<double>> H;
    fft(padded, H);
    return H;
}

} // namespace ofdm
