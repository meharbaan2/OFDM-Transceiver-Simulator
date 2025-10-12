#include <iostream>
#include <vector>
#include <complex>
#include <random>
#include <cmath>
#include <fftw3.h>
#include <algorithm>

using namespace std;

enum class ModScheme {
    QPSK,
    QAM16,
    QAM64
};

// -----------------------------------------------------------------------------
//  Utility: generate random bits
// -----------------------------------------------------------------------------
vector<int> generate_bits(int n) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 1);
    vector<int> bits(n);
    for (int i = 0; i < n; i++) bits[i] = dis(gen);
    return bits;
}

// -----------------------------------------------------------------------------
// Generalized Modulator
// -----------------------------------------------------------------------------
vector<complex<double>> modulate(const vector<int>& bits, ModScheme scheme) {
    vector<complex<double>> symbols;

    if (scheme == ModScheme::QPSK) {
        // QPSK: 2 bits/symbol
        for (size_t i = 0; i < bits.size(); i += 2) {
            double re = (bits[i] == 0) ? -1.0 : 1.0;
            double im = (bits[i + 1] == 0) ? -1.0 : 1.0;
            symbols.emplace_back(re / sqrt(2.0), im / sqrt(2.0));
        }
    }
    else if (scheme == ModScheme::QAM16) {
        // 16-QAM: 4 bits/symbol (Gray mapping)
        const double norm = 1.0 / sqrt(10.0);
        for (size_t i = 0; i < bits.size(); i += 4) {
            int b0 = bits[i], b1 = bits[i + 1], b2 = bits[i + 2], b3 = bits[i + 3];
            int re_level = (2 * b0 + b1) * 2 - 3; // {-3,-1,1,3}
            int im_level = (2 * b2 + b3) * 2 - 3;
            symbols.emplace_back(re_level * norm, im_level * norm);
        }
    }
    else if (scheme == ModScheme::QAM64) {
        // 64-QAM: 6 bits/symbol (Gray mapping)
        const double norm = 1.0 / sqrt(42.0);
        for (size_t i = 0; i < bits.size(); i += 6) {
            int b0 = bits[i], b1 = bits[i + 1], b2 = bits[i + 2];
            int b3 = bits[i + 3], b4 = bits[i + 4], b5 = bits[i + 5];

            int re_level = (4 * b0 + 2 * b1 + b2) * 2 - 7; // {-7,-5,-3,-1,1,3,5,7}
            int im_level = (4 * b3 + 2 * b4 + b5) * 2 - 7;

            symbols.emplace_back(re_level * norm, im_level * norm);
        }
    }

    return symbols;
}

// -----------------------------------------------------------------------------
// Generalized Demodulator
// -----------------------------------------------------------------------------
vector<int> demodulate(const vector<complex<double>>& symbols, ModScheme scheme) {
    vector<int> bits;

    if (scheme == ModScheme::QPSK) {
        for (auto s : symbols) {
            bits.push_back(real(s) > 0 ? 1 : 0);
            bits.push_back(imag(s) > 0 ? 1 : 0);
        }
    }
    else if (scheme == ModScheme::QAM16) {
        const double norm = 1.0 / sqrt(10.0);
        for (auto s : symbols) {
            double re = real(s) / norm;
            double im = imag(s) / norm;

            int re_lev = round((re + 3) / 2); // map {-3,-1,1,3} to {0,1,2,3}
            int im_lev = round((im + 3) / 2);

            re_lev = max(0, min(3, re_lev));
            im_lev = max(0, min(3, im_lev));

            bits.push_back((re_lev >> 1) & 1);
            bits.push_back(re_lev & 1);
            bits.push_back((im_lev >> 1) & 1);
            bits.push_back(im_lev & 1);
        }
    }
    else if (scheme == ModScheme::QAM64) {
        const double norm = 1.0 / sqrt(42.0);
        for (auto s : symbols) {
            double re = real(s) / norm;
            double im = imag(s) / norm;

            int re_lev = round((re + 7) / 2); // map {-7,-5,-3,-1,1,3,5,7} to 0–7
            int im_lev = round((im + 7) / 2);
            re_lev = max(0, min(7, re_lev));
            im_lev = max(0, min(7, im_lev));

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

// -----------------------------------------------------------------------------
//  Add AWGN noise with proper scaling
// -----------------------------------------------------------------------------
vector<complex<double>> awgn(const vector<complex<double>>& signal, double snr_db) {
    // Compute actual average signal power
    double sigPower = 0.0;
    for (auto s : signal) sigPower += norm(s);
    sigPower /= signal.size();

    double snr_lin = pow(10.0, snr_db / 10.0);
    double noise_var = sigPower / snr_lin;
    double sigma = sqrt(noise_var / 2.0);

    random_device rd;
    mt19937 gen(rd());
    normal_distribution<> dist(0.0, sigma);

    vector<complex<double>> noisy(signal.size());
    for (size_t i = 0; i < signal.size(); i++)
        noisy[i] = signal[i] + complex<double>(dist(gen), dist(gen));

    return noisy;
}

// -----------------------------------------------------------------------------
//  OFDM modulation (IFFT + Cyclic Prefix)
// -----------------------------------------------------------------------------
vector<complex<double>> ofdm_mod(const vector<complex<double>>& symbols, int N, int cp_len) {
    vector<complex<double>> tx;
    int num_symbols = symbols.size();
    int num_frames = (num_symbols + N - 1) / N; // ceiling division

    tx.reserve(num_frames * (N + cp_len));

    for (int i = 0; i < num_frames; i++) {
        vector<complex<double>> frame(N, 0.0); // Initialize with zeros

        // Fill frame with available symbols (last frame might be partially filled)
        int symbols_to_copy = min(N, num_symbols - i * N);
        for (int k = 0; k < symbols_to_copy; k++) {
            frame[k] = symbols[i * N + k];
        }

        fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
        fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
        fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);

        for (int k = 0; k < N; k++) {
            in[k][0] = real(frame[k]);
            in[k][1] = imag(frame[k]);
        }

        fftw_execute(p);

        // Add cyclic prefix and normalize by 1/sqrt(N)
        for (int k = N - cp_len; k < N; k++)
            tx.emplace_back(out[k][0] / sqrt(N), out[k][1] / sqrt(N));

        for (int k = 0; k < N; k++)
            tx.emplace_back(out[k][0] / sqrt(N), out[k][1] / sqrt(N));

        fftw_destroy_plan(p);
        fftw_free(in);
        fftw_free(out);
    }

    return tx;
}

// -----------------------------------------------------------------------------
//  OFDM demodulation (remove CP + FFT)
// -----------------------------------------------------------------------------
vector<complex<double>> ofdm_demod(const vector<complex<double>>& rx, int N, int cp_len) {
    vector<complex<double>> symbols;
    int frame_length = N + cp_len;
    int total_frames = rx.size() / frame_length;

    symbols.reserve(total_frames * N);

    for (int i = 0; i < total_frames; i++) {
        size_t frame_start = i * frame_length;

        // Check if we have a complete frame
        if (frame_start + frame_length > rx.size()) {
            break; // Incomplete frame, skip it
        }

        // Extract frame without CP
        vector<complex<double>> frame(N);
        for (int k = 0; k < N; k++) {
            frame[k] = rx[frame_start + cp_len + k];
        }

        fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
        fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
        fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

        for (int k = 0; k < N; k++) {
            in[k][0] = real(frame[k]);
            in[k][1] = imag(frame[k]);
        }

        fftw_execute(p);

        // Undo the 1/sqrt(N) normalization
        for (int k = 0; k < N; k++) {
            symbols.emplace_back(out[k][0] / sqrt(N), out[k][1] / sqrt(N));
        }

        fftw_destroy_plan(p);
        fftw_free(in);
        fftw_free(out);
    }

    return symbols;
}

// -----------------------------------------------------------------------------
// Rayleigh fading channel (multipath convolution)
// -----------------------------------------------------------------------------
vector<complex<double>> rayleigh_fading(const vector<complex<double>>& tx, int num_taps, double snr_db) {
    int N = tx.size();
    int L = num_taps;

    // Generate random Rayleigh fading channel
    vector<complex<double>> h(L);
    random_device rd;
    mt19937 gen(rd());
    normal_distribution<> dist(0.0, 1.0);

    // Generate complex Gaussian taps
    for (int i = 0; i < L; i++) {
        h[i] = complex<double>(dist(gen), dist(gen)) / sqrt(2.0);
    }

    // Normalize channel to unit average power
    double channel_power = 0.0;
    for (int i = 0; i < L; i++) {
        channel_power += norm(h[i]);
    }
    double scale = 1.0 / sqrt(channel_power);
    for (int i = 0; i < L; i++) {
        h[i] *= scale;
    }

    // Linear convolution
    vector<complex<double>> y(N + L - 1, 0.0);
    for (int n = 0; n < N + L - 1; n++) {
        for (int l = 0; l < L; l++) {
            if (n - l >= 0 && n - l < N) {
                y[n] += h[l] * tx[n - l];
            }
        }
    }

    // Calculate PROPER noise variance
    // For unit power channel and unit power symbols, the output power should be ~1
    // So we can directly use the SNR to calculate noise variance
    double snr_lin = pow(10.0, snr_db / 10.0);
    double noise_var = 1.0 / snr_lin;  // Direct calculation for unit power signals
    double sigma = sqrt(noise_var / 2.0);

    normal_distribution<> noise_dist(0.0, sigma);

    // Add noise
    for (auto& sample : y) {
        sample += complex<double>(noise_dist(gen), noise_dist(gen));
    }

    return y;
}

// -----------------------------------------------------------------------------
//  Insert pilot symbols for channel estimation
// -----------------------------------------------------------------------------
vector<complex<double>> insert_pilots(const vector<complex<double>>& data_symbols,
    int N, int pilot_spacing, int num_frames) {
    vector<complex<double>> symbols_with_pilots;

    int pilots_per_frame = (N + pilot_spacing - 1) / pilot_spacing;
    int data_per_frame = N - pilots_per_frame;

    // Better pilot sequence with alternating signs for better estimation
    vector<complex<double>> pilot_sequence(pilots_per_frame);
    for (int i = 0; i < pilots_per_frame; i++) {
        pilot_sequence[i] = (i % 2 == 0) ? complex<double>(1.0, 0.0) : complex<double>(-1.0, 0.0);
    }

    for (int frame = 0; frame < num_frames; frame++) {
        for (int i = 0; i < N; i++) {
            if (i % pilot_spacing == 0) {
                int pilot_idx = i / pilot_spacing;
                symbols_with_pilots.push_back(pilot_sequence[pilot_idx % pilot_sequence.size()]);
            }
            else {
                int data_idx = frame * data_per_frame + (i - (i / pilot_spacing) - 1);
                if (data_idx < data_symbols.size()) {
                    symbols_with_pilots.push_back(data_symbols[data_idx]);
                }
                else {
                    symbols_with_pilots.push_back(0.0);
                }
            }
        }
    }

    return symbols_with_pilots;
}

// -----------------------------------------------------------------------------
//  Channel estimation using pilot symbols
// -----------------------------------------------------------------------------
vector<complex<double>> estimate_channel(const vector<complex<double>>& rx_pilots,
    const vector<complex<double>>& tx_pilots,
    int N, int pilot_spacing) {
    vector<complex<double>> H_estimated(N, 1.0);

    // Step 1: Estimate channel at pilot positions with outlier rejection
    vector<complex<double>> H_pilots(tx_pilots.size(), 0.0);
    vector<double> pilot_magnitudes(tx_pilots.size());

    for (size_t i = 0; i < tx_pilots.size(); i++) {
        if (abs(tx_pilots[i]) > 1e-6) {
            H_pilots[i] = rx_pilots[i] / tx_pilots[i];
            pilot_magnitudes[i] = abs(H_pilots[i]);
        }
    }

    // Step 2: Remove outlier pilots (those with extremely weak/strong channels)
    double median_magnitude = 0.0;
    vector<double> sorted_mags = pilot_magnitudes;
    sort(sorted_mags.begin(), sorted_mags.end());
    if (!sorted_mags.empty()) {
        median_magnitude = sorted_mags[sorted_mags.size() / 2];
    }

    // Step 3: Robust interpolation with outlier protection
    for (int i = 0; i < N; i++) {
        if (i % pilot_spacing == 0) {
            // Pilot position - use robust estimate
            int pilot_idx = i / pilot_spacing;
            if (pilot_idx < (int)H_pilots.size()) {
                // Check if this pilot is an outlier
                double mag = pilot_magnitudes[pilot_idx];
                if (mag > median_magnitude * 10.0 || mag < median_magnitude * 0.1) {
                    // Outlier - use median value instead
                    H_estimated[i] = complex<double>(median_magnitude, 0.0);
                }
                else {
                    H_estimated[i] = H_pilots[pilot_idx];
                }
            }
        }
        else {
            // Robust linear interpolation
            int left_pilot = (i / pilot_spacing) * pilot_spacing;
            int right_pilot = left_pilot + pilot_spacing;

            if (left_pilot < 0) left_pilot = 0;
            if (right_pilot >= N) right_pilot = N - 1;

            int left_idx = left_pilot / pilot_spacing;
            int right_idx = right_pilot / pilot_spacing;

            if (left_idx >= (int)H_pilots.size()) left_idx = H_pilots.size() - 1;
            if (right_idx >= (int)H_pilots.size()) right_idx = H_pilots.size() - 1;

            // Use robust estimates for interpolation
            complex<double> left_H = (pilot_magnitudes[left_idx] < median_magnitude * 0.1 ||
                pilot_magnitudes[left_idx] > median_magnitude * 10.0) ?
                complex<double>(median_magnitude, 0.0) : H_pilots[left_idx];

            complex<double> right_H = (pilot_magnitudes[right_idx] < median_magnitude * 0.1 ||
                pilot_magnitudes[right_idx] > median_magnitude * 10.0) ?
                complex<double>(median_magnitude, 0.0) : H_pilots[right_idx];

            double t = (double)(i - left_pilot) / (right_pilot - left_pilot);
            H_estimated[i] = (1.0 - t) * left_H + t * right_H;
        }
    }

    // Step 4: Enhanced frequency domain smoothing
    vector<complex<double>> H_smoothed(N);
    int smooth_window = 5; // Increased window for better smoothing

    for (int i = 0; i < N; i++) {
        complex<double> sum = 0.0;
        double weight_sum = 0.0;

        for (int j = -smooth_window; j <= smooth_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < N) {
                // Weighted smoothing - give more weight to center
                double weight = 1.0 / (1.0 + abs(j)); // Triangular weighting
                sum += H_estimated[idx] * weight;
                weight_sum += weight;
            }
        }

        H_smoothed[i] = sum / weight_sum;
    }

    H_estimated = H_smoothed;

    return H_estimated;
}

// -----------------------------------------------------------------------------
//  Frequency domain equalization
// -----------------------------------------------------------------------------
vector<complex<double>> equalize_symbols(const vector<complex<double>>& rx_symbols,
    const vector<complex<double>>& H_estimated,
    double snr_db = 20.0) {
    vector<complex<double>> equalized_symbols(rx_symbols.size());

    double snr_lin = pow(10.0, snr_db / 10.0);
    double noise_var = 1.0 / snr_lin;

    // Calculate average channel power for better equalization
    double avg_channel_power = 0.0;
    for (int i = 0; i < H_estimated.size(); i++) {
        avg_channel_power += norm(H_estimated[i]);
    }
    avg_channel_power /= H_estimated.size();

    // Adaptive noise variance based on channel conditions
    double effective_noise_var = noise_var / avg_channel_power;

    for (size_t i = 0; i < rx_symbols.size(); i++) {
        complex<double> H = H_estimated[i % H_estimated.size()];
        complex<double> H_conj = conj(H);
        double H_power = norm(H);

        // MMSE equalization with channel-aware regularization
        complex<double> equalizer_weight = H_conj / (H_power + effective_noise_var + 1e-12);
        equalized_symbols[i] = equalizer_weight * rx_symbols[i];
    }

    return equalized_symbols;
}

// -----------------------------------------------------------------------------
//  Extract pilot symbols from received signal
// -----------------------------------------------------------------------------
vector<complex<double>> extract_pilots(const vector<complex<double>>& symbols,
    int N, int pilot_spacing) {
    vector<complex<double>> pilots;

    for (int i = 0; i < N; i++) {
        if (i % pilot_spacing == 0) {
            pilots.push_back(symbols[i]);
        }
    }

    return pilots;
}

// -----------------------------------------------------------------------------
//  Extract data symbols by removing pilots
// -----------------------------------------------------------------------------
vector<complex<double>> extract_data(const vector<complex<double>>& symbols_with_pilots,
    int N, int pilot_spacing) {
    vector<complex<double>> data_symbols;
    int num_frames = symbols_with_pilots.size() / N;
    int pilots_per_frame = (N + pilot_spacing - 1) / pilot_spacing;
    int data_per_frame = N - pilots_per_frame;

    for (int frame = 0; frame < num_frames; frame++) {
        for (int i = 0; i < N; i++) {
            if (i % pilot_spacing != 0) {
                int symbol_idx = frame * N + i;
                if (symbol_idx < symbols_with_pilots.size()) {
                    data_symbols.push_back(symbols_with_pilots[symbol_idx]);
                }
            }
        }
    }

    return data_symbols;
}

// -----------------------------------------------------------------------------
//  Zero-Forcing equalization with threshold
// -----------------------------------------------------------------------------
vector<complex<double>> zf_equalize_symbols(const vector<complex<double>>& rx_symbols,
    const vector<complex<double>>& H_estimated) {
    vector<complex<double>> equalized_symbols(rx_symbols.size());

    double threshold = 0.1; // Minimum channel magnitude

    for (size_t i = 0; i < rx_symbols.size(); i++) {
        complex<double> H = H_estimated[i % H_estimated.size()];
        double H_mag = abs(H);

        if (H_mag > threshold) {
            // Normal ZF equalization
            equalized_symbols[i] = rx_symbols[i] / H;
        }
        else {
            // For very weak subcarriers, use attenuated version
            equalized_symbols[i] = rx_symbols[i] * (H_mag / threshold);
        }
    }

    return equalized_symbols;
}

// -----------------------------------------------------------------------------
//  Simple scaling equalizer as last resort
// -----------------------------------------------------------------------------
vector<complex<double>> simple_equalize(const vector<complex<double>>& rx_symbols,
    const vector<complex<double>>& H_estimated) {
    vector<complex<double>> equalized_symbols(rx_symbols.size());

    // Calculate channel statistics for better threshold
    double avg_magnitude = 0.0;
    for (int i = 0; i < H_estimated.size(); i++) {
        avg_magnitude += abs(H_estimated[i]);
    }
    avg_magnitude /= H_estimated.size();

    double threshold = avg_magnitude * 0.3; // Adaptive threshold

    for (size_t i = 0; i < rx_symbols.size(); i++) {
        complex<double> H = H_estimated[i % H_estimated.size()];
        double H_mag = abs(H);

        if (H_mag > threshold) {
            // Normal equalization for good channels
            equalized_symbols[i] = rx_symbols[i] / H;
        }
        else {
            // For weak channels, use conservative approach
            // Scale based on channel strength to avoid noise amplification
            equalized_symbols[i] = rx_symbols[i] * (H_mag / (H_mag + threshold));
        }
    }

    return equalized_symbols;
}

// -----------------------------------------------------------------------------
//  Debug: Print constellation points
// -----------------------------------------------------------------------------
void print_constellation(const vector<complex<double>>& symbols, int num_points = 10) {
    cout << "First " << num_points << " constellation points:" << endl;
    for (int i = 0; i < min(num_points, (int)symbols.size()); i++) {
        cout << "  (" << real(symbols[i]) << ", " << imag(symbols[i]) << ")" << endl;
    }
}

// -----------------------------------------------------------------------------
//  Debug signal powers at each stage
// -----------------------------------------------------------------------------
void debug_signal_powers(const vector<complex<double>>& data_symbols,
    const vector<complex<double>>& tx_signal,
    const vector<complex<double>>& rx_signal,
    const vector<complex<double>>& rx_symbols) {
    double data_power = 0.0, tx_power = 0.0, rx_power = 0.0, sym_power = 0.0;

    for (auto s : data_symbols) data_power += norm(s);
    for (auto s : tx_signal) tx_power += norm(s);
    for (auto s : rx_signal) rx_power += norm(s);
    for (auto s : rx_symbols) sym_power += norm(s);

    data_power /= data_symbols.size();
    tx_power /= tx_signal.size();
    rx_power /= rx_signal.size();
    sym_power /= rx_symbols.size();

    cout << "\n=== SIGNAL POWER ANALYSIS ===" << endl;
    cout << "Data symbols power: " << data_power << endl;
    cout << "TX signal power: " << tx_power << endl;
    cout << "RX signal power: " << rx_power << endl;
    cout << "RX symbols power: " << sym_power << endl;
    cout << "Channel gain: " << (sym_power / data_power) << endl;
}

// -----------------------------------------------------------------------------
//  Constellation-based SNR estimation
// -----------------------------------------------------------------------------
double estimate_snr_from_constellation(const vector<complex<double>>& symbols,
    ModScheme scheme) {
    if (symbols.empty()) return 0.0;

    vector<complex<double>> ideal_points;

    if (scheme == ModScheme::QPSK) {
        ideal_points = {
            complex<double>(1 / sqrt(2.0),  1 / sqrt(2.0)),
            complex<double>(1 / sqrt(2.0), -1 / sqrt(2.0)),
            complex<double>(-1 / sqrt(2.0),  1 / sqrt(2.0)),
            complex<double>(-1 / sqrt(2.0), -1 / sqrt(2.0))
        };
    }
    else if (scheme == ModScheme::QAM16) {
        const double norm = 1.0 / sqrt(10.0);
        for (int i = -3; i <= 3; i += 2) {
            for (int j = -3; j <= 3; j += 2) {
                ideal_points.emplace_back(i * norm, j * norm);
            }
        }
    }
    else if (scheme == ModScheme::QAM64) {
        const double norm = 1.0 / sqrt(42.0);
        for (int i = -7; i <= 7; i += 2) {
            for (int j = -7; j <= 7; j += 2) {
                ideal_points.emplace_back(i * norm, j * norm);
            }
        }
    }

    double total_noise_power = 0.0;
    int count = 0;

    // Use a reasonable sample size
    int max_samples = min(1000, (int)symbols.size());

    for (int idx = 0; idx < max_samples; idx++) {
        const auto& sym = symbols[idx];

        // Find closest ideal constellation point
        double min_distance = 1e9;
        complex<double> closest_point;

        for (const auto& ideal : ideal_points) {
            double distance = norm(sym - ideal);
            if (distance < min_distance) {
                min_distance = distance;
                closest_point = ideal;
            }
        }

        // Noise power is distance to closest ideal point
        total_noise_power += min_distance;
        count++;
    }

    if (count == 0 || total_noise_power < 1e-12) {
        return 1000.0; // Return high SNR instead of infinity
    }

    double avg_noise_power = total_noise_power / count;
    double signal_power = 1.0; // Assuming normalized constellation

    return signal_power / avg_noise_power;
}

// -----------------------------------------------------------------------------
//  Main simulation
// -----------------------------------------------------------------------------
int main() {
    int N = 64;         // OFDM subcarriers
    int cp_len = 16;    // cyclic prefix length
    int pilot_spacing = 8; // Insert pilot every 8 subcarriers

    // User-selectable parameters
    int channel_choice, modulation_choice, equalizer_choice;
    double snr_db;
    int num_taps = 4;

    // Get user input
    cout << "=== OFDM System Configuration ===" << endl;
    cout << "Select Channel Model:" << endl;
    cout << "1. AWGN (Additive White Gaussian Noise)" << endl;
    cout << "2. Rayleigh Fading (Multipath)" << endl;
    cout << "3. No Channel (Ideal - for testing)" << endl;
    cout << "Choice (1-3): ";
    cin >> channel_choice;

    if (cin.fail()) {
        cout << "Error: Invalid input. Must be a number. Using AWGN as default." << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        channel_choice = 1;
    }
    else if (channel_choice < 1 || channel_choice > 3) {
        cout << "Error: Invalid channel choice. Must be between 1 and 3. Using AWGN as default." << endl;
        channel_choice = 1;
    }

    cout << "\nSelect Modulation Scheme:" << endl;
    cout << "1. QPSK (2 bits/symbol)" << endl;
    cout << "2. 16-QAM (4 bits/symbol)" << endl;
    cout << "3. 64-QAM (6 bits/symbol)" << endl;
    cout << "Choice (1-3): ";
    cin >> modulation_choice;

    if (cin.fail()) {
        cout << "Error: Invalid input. Must be a number. Using QPSK as default." << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        modulation_choice = 1;
    }
    else if (modulation_choice < 1 || modulation_choice > 3) {
        cout << "Error: Invalid modulation choice. Must be between 1 and 3. Using QPSK as default." << endl;
        modulation_choice = 1;
    }

    cout << "\nSelect Equalizer Type:" << endl;
    cout << "1. MMSE (Minimum Mean Square Error)" << endl;
    cout << "2. ZF (Zero Forcing)" << endl;
    cout << "3. Simple Equalizer" << endl;
    cout << "Choice (1-3): ";
    cin >> equalizer_choice;

    if (cin.fail()) {
        cout << "Error: Invalid input. Must be a number. Using MMSE as default." << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        equalizer_choice = 1;
    }
    else if (equalizer_choice < 1 || equalizer_choice > 3) {
        cout << "Error: Invalid equalizer choice. Must be between 1 and 3. Using MMSE as default." << endl;
        equalizer_choice = 1;
    }

    cout << "\nEnter SNR (dB): ";
    cin >> snr_db;

    if (cin.fail()) {
        cout << "Error: Invalid SNR input. Must be a number. Using 20 dB as default." << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        snr_db = 20.0;
    }
    else if (snr_db < -20.0 || snr_db > 50.0) {
        cout << "Warning: SNR " << snr_db << " dB is outside typical range [-20, 50] dB. Continuing anyway." << endl;
    }

    if (channel_choice == 2) {
        cout << "Enter number of channel taps for Rayleigh fading: ";
        cin >> num_taps;

        if (cin.fail()) {
            cout << "Error: Invalid taps input. Must be a number. Using 4 taps as default." << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            num_taps = 4;
        }
        else if (num_taps < 1) {
            cout << "Error: Number of taps must be at least 1. Using 4 taps as default." << endl;
            num_taps = 4;
        }
        else if (num_taps > 16) {
            cout << "Warning: " << num_taps << " taps may cause excessive delay spread. Consider using fewer taps." << endl;
        }
    }

    // Set modulation scheme based on user choice
    ModScheme scheme;
    string scheme_name;
    switch (modulation_choice) {
    case 1:
        scheme = ModScheme::QPSK;
        scheme_name = "QPSK";
        break;
    case 2:
        scheme = ModScheme::QAM16;
        scheme_name = "16-QAM";
        break;
    case 3:
        scheme = ModScheme::QAM64;
        scheme_name = "64-QAM";
        break;
    default:
        cout << "Invalid modulation choice, using QPSK" << endl;
        scheme = ModScheme::QPSK;
        scheme_name = "QPSK";
    }

    int bits_per_symbol = (scheme == ModScheme::QPSK) ? 2 :
        (scheme == ModScheme::QAM16) ? 4 : 6;

    // Calculate proper sizes
    int pilots_per_frame = (N + pilot_spacing - 1) / pilot_spacing;
    int data_per_frame = N - pilots_per_frame;
    int num_frames = 100;
    int num_data_symbols = num_frames * data_per_frame;
    int num_bits = num_data_symbols * bits_per_symbol;

    cout << "\n=== OFDM System Parameters ===" << endl;
    cout << "Subcarriers: " << N << ", CP: " << cp_len << endl;
    cout << "Pilots per frame: " << pilots_per_frame << ", Data per frame: " << data_per_frame << endl;
    cout << "Number of frames: " << num_frames << endl;
    cout << "Total data symbols: " << num_data_symbols << endl;
    cout << "Total bits: " << num_bits << endl;
    cout << "Modulation: " << scheme_name << endl;
    cout << "Channel: " << (channel_choice == 1 ? "AWGN" : "Rayleigh Fading") << endl;
    cout << "SNR: " << snr_db << " dB" << endl;

    // 1. Transmitter
    auto bits = generate_bits(num_bits);
    auto data_symbols = modulate(bits, scheme);

    // Insert pilots for channel estimation
    auto symbols_with_pilots = insert_pilots(data_symbols, N, pilot_spacing, num_frames);
    auto tx_signal = ofdm_mod(symbols_with_pilots, N, cp_len);

    // Create known pilot sequence for receiver
    vector<complex<double>> tx_pilot_sequence(pilots_per_frame);
    for (int i = 0; i < pilots_per_frame; i++) {
        tx_pilot_sequence[i] = (i % 2 == 0) ? complex<double>(1.0, 0.0) : complex<double>(-1.0, 0.0);
    }

    cout << "\n=== Transmitter ===" << endl;
    cout << "Data symbols: " << data_symbols.size() << endl;
    cout << "Symbols with pilots: " << symbols_with_pilots.size() << endl;
    cout << "Transmitted signal length: " << tx_signal.size() << endl;

    // 2. Channel
    vector<complex<double>> rx_signal;
    if (channel_choice == 1) {
        rx_signal = awgn(tx_signal, snr_db);
        cout << "\n=== Channel: AWGN ===" << endl;
    }
    else if (channel_choice == 2) {
        rx_signal = rayleigh_fading(tx_signal, num_taps, snr_db);
        cout << "\n=== Channel: Rayleigh Fading (" << num_taps << " taps) ===" << endl;
	}
	else if (channel_choice == 3) {
		rx_signal = tx_signal; // No channel
		cout << "\n=== Channel: Ideal (No channel effects) ===" << endl;
	}
    else {
        cout << "Invalid choice! Using AWGN as default." << endl;
        rx_signal = awgn(tx_signal, snr_db);
    }
    cout << "Received signal length: " << rx_signal.size() << endl;

    // 3. Receiver
    auto rx_symbols_with_pilots = ofdm_demod(rx_signal, N, cp_len);

    cout << "\n=== Receiver ===" << endl;
    cout << "Received symbols with pilots: " << rx_symbols_with_pilots.size() << endl;

    // Signal power analysis
    debug_signal_powers(data_symbols, tx_signal, rx_signal, rx_symbols_with_pilots);

    // Channel estimation and equalization per frame
    vector<complex<double>> all_equalized_symbols;
    int frames_processed = min(num_frames, (int)rx_symbols_with_pilots.size() / N);

    for (int frame = 0; frame < frames_processed; frame++) {
        // Extract current frame
        vector<complex<double>> frame_symbols(N);
        for (int i = 0; i < N; i++) {
            int idx = frame * N + i;
            if (idx < rx_symbols_with_pilots.size()) {
                frame_symbols[i] = rx_symbols_with_pilots[idx];
            }
        }

        // Channel estimation for this frame
        auto rx_pilots = extract_pilots(frame_symbols, N, pilot_spacing);
        auto H_estimated = estimate_channel(rx_pilots, tx_pilot_sequence, N, pilot_spacing);

        // Debug information for first frame
        if (frame == 0) {
            vector<double> magnitudes(N);
            for (int i = 0; i < N; i++) {
                magnitudes[i] = abs(H_estimated[i]);
            }
            sort(magnitudes.begin(), magnitudes.end());

            cout << "\n=== Channel State Information (Frame 0) ===" << endl;
            cout << "Channel magnitude statistics:" << endl;
            cout << "  Min: " << magnitudes[0] << endl;
            cout << "  10%: " << magnitudes[N / 10] << endl;
            cout << "  Median: " << magnitudes[N / 2] << endl;
            cout << "  90%: " << magnitudes[N * 9 / 10] << endl;
            cout << "  Max: " << magnitudes[N - 1] << endl;

            int weak_count = 0;
            for (int i = 0; i < N; i++) {
                if (magnitudes[i] < 0.1) weak_count++;
            }
            cout << "Weak subcarriers (|H| < 0.1): " << weak_count << "/" << N << endl;
        }

        // Equalization based on user choice
        vector<complex<double>> equalized_frame;
        string equalizer_name;

        switch (equalizer_choice) {
        case 1:
            equalized_frame = equalize_symbols(frame_symbols, H_estimated, snr_db);
            equalizer_name = "MMSE";
            break;
        case 2:
            equalized_frame = zf_equalize_symbols(frame_symbols, H_estimated);
            equalizer_name = "ZF";
            break;
        case 3:
            equalized_frame = simple_equalize(frame_symbols, H_estimated);
            equalizer_name = "Simple";
            break;
        default:
            equalized_frame = equalize_symbols(frame_symbols, H_estimated, snr_db);
            equalizer_name = "MMSE";
        }

        // Power normalization
        double frame_power = 0.0;
        for (const auto& sym : equalized_frame) {
            frame_power += norm(sym);
        }
        frame_power /= equalized_frame.size();
        double scale = 1.0 / sqrt(frame_power);
        for (auto& sym : equalized_frame) {
            sym *= scale;
        }

        // Debug for first frame
        if (frame == 0) {
            cout << "\n=== Equalization Debug (Frame 0) ===" << endl;
            cout << "Equalizer: " << equalizer_name << endl;
            cout << "Equalized frame power: " << frame_power << endl;
            cout << "First 5 equalized symbols:" << endl;
            for (int i = 0; i < min(5, (int)equalized_frame.size()); i++) {
                cout << "  (" << real(equalized_frame[i]) << ", " << imag(equalized_frame[i]) << ")" << endl;
            }
        }

        // Extract data from this frame
        auto frame_data = extract_data(equalized_frame, N, pilot_spacing);
        all_equalized_symbols.insert(all_equalized_symbols.end(),
            frame_data.begin(), frame_data.end());
    }

    // Demodulate
    auto rx_bits = demodulate(all_equalized_symbols, scheme);

    cout << "\n=== System Performance Summary ===" << endl;
    cout << "Frames processed: " << frames_processed << " out of " << num_frames << endl;
    cout << "Equalized data symbols: " << all_equalized_symbols.size() << endl;
    cout << "Received bits: " << rx_bits.size() << endl;

    // Final signal quality
    if (!all_equalized_symbols.empty()) {
        double estimated_snr_linear = estimate_snr_from_constellation(all_equalized_symbols, scheme);
        double estimated_snr_db = 10.0 * log10(estimated_snr_linear);

        cout << "Constellation-based SNR estimation:" << endl;
        cout << "Estimated SNR: " << estimated_snr_linear << " linear, "
            << estimated_snr_db << " dB" << endl;
    }

    // BER computation
    int errors = 0;
    int min_size = min(bits.size(), rx_bits.size());
    for (size_t i = 0; i < min_size; i++) {
        if (bits[i] != rx_bits[i]) errors++;
    }

    double ber = (double)errors / min_size;
    cout << "\n=== Results ===" << endl;
    cout << "Modulation: " << scheme_name << endl;
    cout << "Channel: " << (channel_choice == 1 ? "AWGN" : "Rayleigh Fading") << endl;
    cout << "Equalizer: " << (equalizer_choice == 1 ? "MMSE" : (equalizer_choice == 2 ? "ZF" : "Simple")) << endl;
    cout << "Target SNR: " << snr_db << " dB" << endl;
    cout << "BER: " << ber << endl;
    cout << "Bits compared: " << min_size << " out of " << bits.size() << endl;
    cout << "Errors: " << errors << endl;

    return 0;
}