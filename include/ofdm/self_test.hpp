#pragma once

namespace ofdm {

// Runs automated checks (FFT round-trip, mod/demod, ideal channel, pilot LS, high-SNR AWGN).
// Returns number of failed tests (0 = all passed).
int run_self_tests();

} // namespace ofdm
