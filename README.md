# OFDM wireless link simulator (C++)

### Overview

This project is an **orthogonal frequency-division multiplexing (OFDM)** baseband simulator in **C++**. It models a full **transmit → channel → receive** chain: modulation, IFFT/FFT-based OFDM, pilots, channel estimation, **MMSE** and **ZF** equalization, and **bit error rate (BER)** versus SNR.

You can exercise:

- **AWGN** only, **frequency-selective Rayleigh** fading (per OFDM symbol) plus AWGN, or an **ideal** noiseless channel  
- **QPSK**, **16-QAM**, **64-QAM**  
- **Pilot-aided least-squares channel estimation** with linear interpolation across subcarriers  

The structure matches the kind of **link-level PHY** model used to study Wi‑Fi / cellular–style OFDM before hardware.

---

### Features

| Area | What is implemented |
|------|---------------------|
| OFDM | IFFT at TX, cyclic prefix, FFT at RX; unitary-style \(1/\sqrt{N}\) scaling on FFT/IFFT (FFTW3) |
| Channel | AWGN; Rayleigh as **per-symbol** frequency response \(H[k]\) from a zero-padded CIR (consistent with CP-OFDM when CP covers delay spread) |
| Estimation | Comb pilots, LS at pilots, complex linear interpolation |
| Equalization | Per-subcarrier **MMSE** and **regularized ZF** |
| Analysis | BER; optional CSV **SNR sweep** from the command line |
| Quality | Built-in **`test`** mode with automated checks |

**Not in the current code path:** a third “Simple” equalizer, per-frame debug prints, and constellation-based SNR estimation (those belonged to an earlier single-file version).

---

### Signal flow

```text
Bits → Modulate (QPSK/QAM) → Insert pilots → OFDM mod (IFFT + CP)
    → Channel (AWGN / Rayleigh / ideal) → OFDM demod (remove CP + FFT)
    → Channel estimate → Equalize (MMSE or ZF) → Demap → BER
```

---

### Example (interactive)

The UI is shorter than in older builds; a typical run looks like this:

```text
=== OFDM System Configuration ===
Channel: 1=AWGN  2=Rayleigh (freq-selective)  3=Ideal
Choice: 1
Modulation: 1=QPSK  2=16-QAM  3=64-QAM
Choice: 2
Equalizer: 1=MMSE  2=ZF
Choice: 1
SNR (dB): 20

--- Parameters ---
N=64, CP=16, pilot spacing=8, frames=100
Modulation: 16-QAM
Channel: AWGN  Equalizer: MMSE  SNR=20 dB

Frames processed: 100/100
BER: 0.000000  (0 errors / 22400 bits)
```

(Exact BER depends on the random bit stream and SNR.)

---

### Requirements

- **C++17** or newer  
- **FFTW3** (headers + import library + DLL on Windows)  
- Toolchain: **MSVC** (recommended here), or **GCC/Clang** via CMake  

---

### Project layout

```text
OFDMSim/
  include/ofdm/     Headers (namespace ofdm)
  src/              Sources and main.cpp
  OFDMSim.sln       Visual Studio solution
  CMakeLists.txt    Optional CMake (set FFTW_ROOT)
```

---

### Build (Visual Studio, Windows x64)

1. Open **`OFDMSim.sln`** from the **`OFDMSim`** folder (not only the parent `C++` folder).  
   - If VS shows **Miscellaneous Files → OFDMSim.cpp** (“file deleted”), close that tab: the project was refactored; entry point is **`src/main.cpp`**. You can delete the **`.vs`** folder while VS is closed to clear stale tabs.  
   - If the solution fails to load, check that **`OFDMSim.sln`** has a single token on the `VisualStudioVersion` line (no stray text after the version number).

2. **FFTW:** the project expects a sibling folder **`../fftw-3.3.5-dll64`** (relative to `OFDMSim/`) containing `fftw3.h` and `libfftw3-3.lib`. Paths are **`OfdmFftwInc`** / **`OfdmFftwLib`** in **`OFDMSim.vcxproj`** — adjust if your FFTW location differs.

3. Select configuration **x64** (not Win32; FFTW layout here is 64-bit), then **Debug** or **Release**.

4. A **post-build** step copies **`libfftw3-3.dll`** next to **`OFDMSim.exe`** when that DLL exists under the FFTW directory.

---

### Build (CMake)

```bash
cmake -S . -B build -DFFTW_ROOT=/path/to/fftw
cmake --build build
```

`FFTW_ROOT` must contain `fftw3.h` and the library appropriate for your platform.

---

### Run

```text
OFDMSim.exe                 Interactive prompts
OFDMSim.exe test            Automated self-tests (exit code 0 = all passed)
OFDMSim.exe sweep <ch> <mod> <eq> [taps]
OFDMSim.exe --help
```

**Sweep:** `ch` = 1 AWGN, 2 Rayleigh, 3 ideal; `mod` = 1 QPSK, 2 16-QAM, 3 64-QAM; `eq` = 1 MMSE, 2 ZF; optional `taps` for Rayleigh (default 4). Prints CSV: `snr_db,ber,errors,bits`.

---

### Self-tests

```text
OFDMSim.exe test
```

Runs FFT round-trip, mod/demod, pilot LS sanity, end-to-end ideal and high-SNR checks, and a Rayleigh smoke test. Use after changing the PHY or build.

---

### Model notes (read before trusting numbers)

- **Rayleigh:** applied in the **frequency domain per OFDM symbol** (\(Y[k]=H[k]X[k]\) before the IFFT). This matches the usual CP-OFDM circular model when **`cp_len`** is large enough versus the channel length (use **`cp_len ≥ rayleigh_taps`** in practice).  
- **MMSE** uses a **scalar** \(\sigma^2\) from the configured SNR; it is not per-tone noise tracking.  
- **Pilots:** linear interpolation only — no MMSE or DFT-based smoothing.  
- No timing offset, no CFO, no coding — BER is **uncoded**.

---

### License / third party

FFTW is distributed under its own license; ship and attribute it according to [FFTW’s terms](http://fftw.org) when redistributing binaries.
