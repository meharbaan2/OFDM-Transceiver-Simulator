# OFDM simulation (C++)

End-to-end baseband simulation of an OFDM link with QPSK / 16-QAM / 64-QAM, FFT-based modulation (FFTW3), pilot-aided channel estimation, MMSE and ZF per-subcarrier equalization, and BER counting.

## Model (what is simulated correctly)

- **OFDM**: IFFT at the transmitter, cyclic prefix, FFT at the receiver. DFT uses a unitary-style scaling: each FFT and IFFT applies a \(1/\sqrt{N}\) factor so that Parseval’s relation matches the implementation.
- **AWGN**: Complex white Gaussian noise after the time-domain signal is formed. The reported SNR is **\(10\log_{10}(P_{\text{signal}}/ \sigma_n^2)\)** where \(P_{\text{signal}}\) is the average **per-sample** energy of that complex baseband waveform and \(\sigma_n^2\) is the noise variance on \(|n|^2\) for \(n\) complex circular Gaussian.
- **Frequency-selective Rayleigh**: For **each OFDM symbol**, a length-\(L\) complex Gaussian CIR is drawn, normalized to unit energy, zero-padded to \(N\), and converted to a per-subcarrier gain \(H[k]\) with the **same** FFT convention as the receiver. The channel acts as **\(Y[k]=H[k]X[k]\)** in the frequency domain before the IFFT. This matches a **circular** convolution model and is consistent with OFDM when the CP length is at least the channel delay spread in samples (practically, keep **`cp_len ≥ rayleigh_taps`**).
- **Pilots**: Comb pilots every `pilot_spacing` subcarriers. Channel estimate is **least-squares at pilots** and **linear interpolation** of the complex gains between pilots.
- **Equalizers** (per subcarrier \(k\)):
  - **MMSE**: \(W_k = H_k^* / (|H_k|^2 + \sigma^2)\) with \(\sigma^2 = 10^{-\mathrm{SNR}_{\mathrm{dB}}/10}\), assuming roughly unit average symbol energy before the noise (reasonable for the normalized constellations used here).
  - **ZF (regularized)**: \(Y_k H_k^* / \max(|H_k|^2,\varepsilon^2)\) to limit noise explosion on deep fades.

### Limitations (read this before trusting numbers)

- MMSE uses a **single** \(\sigma^2\) from the configured SNR, not a per-tone noise estimate; with strong fading this is an approximation.
- Pilot interpolation is **linear in frequency**, not MMSE or DFT-based smoothing; performance can be improved with better estimators.
- No timing or carrier-frequency offset, no clipping, no coding.

## Layout

```
OFDMSim/
  include/ofdm/     Public headers (namespace ofdm)
  src/              Implementation + main.cpp
  CMakeLists.txt    Optional CMake build (needs FFTW)
  OFDMSim.sln     Visual Studio solution
```

| Module | Role |
|--------|------|
| `ofdm_engine` | FFTW plans, FFT/IFFT, CIR → \(H[k]\) |
| `ofdm_frame` | OFDM mod/demod, Rayleigh in frequency |
| `channel_awgn` | AWGN |
| `modulation` | Bit ↔ symbol maps |
| `pilots` | Pilot grid, LS + interpolate |
| `equalizer` | MMSE / ZF |
| `simulation` | `run_ofdm_sim` |
| `self_test` | Automated checks |

## Build (Visual Studio, Windows x64)

1. Open **`OFDMSim/OFDMSim.sln`** (inside the `OFDMSim` project folder, not the parent `C++` folder). If Visual Studio refuses to load the solution, ensure `OFDMSim.sln` has not been hand-edited with extra text on the `VisualStudioVersion` line.
   - The simulator was split out of a single `OFDMSim.cpp` into `src/*.cpp`. If VS shows **Miscellaneous Files → OFDMSim.cpp** and “file was deleted”, close that tab (or delete the `.vs` folder inside `OFDMSim` while VS is closed) and use **Solution Explorer → `src/main.cpp`** as the entry point.
2. Install **FFTW** for Windows (this repo assumes a sibling folder `../fftw-3.3.5-dll64` with `fftw3.h` and `libfftw3-3.lib`, relative to `OFDMSim/`). Adjust `OfdmFftwInc` / `OfdmFftwLib` in `OFDMSim.vcxproj` if your layout differs.
3. Select **x64**, build **Debug** or **Release**.
4. The **x64** Debug/Release builds run a post-build step that copies `libfftw3-3.dll` next to `OFDMSim.exe` when it exists under `FFTW` (see `OfdmFftwLib` in the project). If copy fails, add the FFTW `bin` folder to **PATH** or copy the DLL manually.

**Note:** Win32 configurations in the project are not wired to FFTW (the prebuilt DLL set here is 64-bit).

## Build (CMake)

```bash
cmake -S . -B build -DFFTW_ROOT=/path/to/fftw
cmake --build build
```

`FFTW_ROOT` must contain `fftw3.h` and the library (`fftw3` for Unix-style linking). On Windows with the same vendor layout as above, point `FFTW_ROOT` at that directory.

## Run

```text
OFDMSim.exe                 Interactive prompts
OFDMSim.exe test            Automated self-tests (exit code 0 = all passed)
OFDMSim.exe sweep <ch> <mod> <eq> [taps]
```

Sweep arguments: `ch` 1=AWGN, 2=Rayleigh, 3=ideal; `mod` 1=QPSK, 2=16-QAM, 3=64-QAM; `eq` 1=MMSE, 2=ZF; optional `taps` for Rayleigh (default 4). Output is CSV: `snr_db,ber,errors,bits`.

## Tests

`OFDMSim test` runs:

1. FFT/IFFT numerical round-trip  
2. Mod/demod for all three schemes  
3. Pilot LS on an ideal (noiseless) frame → \(\hat H \approx 1\)  
4. End-to-end ideal channel → BER = 0  
5. AWGN at 45 dB → BER = 0 (smoke)  
6. Single-tone OFDM frame → identity through CP + FFT  
7. Rayleigh + AWGN smoke (no crash, bits compared)

Run this after changing the PHY or build flags.

## Why the old “full stream” Rayleigh was wrong

Convolving the **entire** time-domain OFDM waveform with a multipath channel mixes adjacent OFDM symbols. Unless you implement a full CP-aware tapped-delay line **per symbol**, the receiver’s “strip CP + FFT” block is no longer matched to the channel. Applying **\(H[k]\) per symbol in frequency** before the IFFT is the standard discrete model consistent with CP-OFDM when the CP covers the impulse response.

---

If you move the FFTW tree, update `OfdmFftwInc` / `OfdmFftwLib` in `OFDMSim.vcxproj` or pass `FFTW_ROOT` to CMake.
