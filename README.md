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
| Analysis | BER; **Es/N0** and **Eb/N0** (from configured SNR, Gray bits/symbol); CSV **sweep** with optional file output |
| Workflow | **`run`** with flags or **INI**; **`sweep`** with SNR range; **`scripts/plot_ber.py`** for curves |
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
  examples/         Sample INI (e.g. sim_default.ini)
  scripts/          plot_ber.py (matplotlib)
  OFDMSim.sln       Visual Studio solution
  CMakeLists.txt    Optional CMake (set FFTW_ROOT)
```

---

### Build (Visual Studio, Windows x64)

1. Open **`OFDMSim.sln`** from the **`OFDMSim`** folder (not only the parent `C++` folder).  
   - If VS shows **Miscellaneous Files → OFDMSim.cpp** (“file deleted”), close that tab: the project was refactored; entry point is **`src/main.cpp`**. You can delete the **`.vs`** folder while VS is closed to clear stale tabs.  
   - If the solution fails to load, check that **`OFDMSim.sln`** has a single token on the `VisualStudioVersion` line (no stray text after the version number).

2. **FFTW:** the project expects a sibling folder **`../fftw-3.3.5-dll64`** (relative to `OFDMSim/`) containing `fftw3.h` and `libfftw3-3.lib`. Paths are **`OfdmFftwInc`** / **`OfdmFftwLib`** in **`OFDMSim.vcxproj`** — adjust if your FFTW location differs.

3. Select configuration **x64** (not Win32; FFTW layout here is 64-bit), then **Debug** or **Release**. The project uses **ISO C++17** (`/std:c++17`).

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
OFDMSim.exe                       Interactive prompts
OFDMSim.exe test                  Self-tests (exit 0 = pass)
OFDMSim.exe run [flags]           One simulation (reproducible with --seed)
OFDMSim.exe sweep <ch> <mod> <eq> [taps] [options]
OFDMSim.exe sweep --ch N --mod N --eq N ...
OFDMSim.exe --help
```

**`run`** — Set parameters with flags and/or **`--config` / `-c`** (INI `key=value` file; repeat `--config` to layer files; later flags override). Keys match the interactive defaults: `N`, `cp_len`, `pilot_spacing`, `num_frames`, `channel`, `modulation` (1–3), `equalizer` (1–2), `snr_db`, `seed` (0 = random), `rayleigh_taps`, `verbose` (true/false). Hyphens in flags are accepted (`--snr-db`, `--cp-len`, …).

- **`--quiet` / `-q`**: one line to stdout: `ber errors bits es_n0_db eb_n0_db` (good for scripts).
- Otherwise: human-readable summary including **Es/N0** and **Eb/N0**. The configured SNR is interpreted as **Es/N0**; **Eb/N0 (dB)** is **Es/N0 (dB)** minus `10*log10(bps)` (bps = bits per symbol).

Example:

```text
OFDMSim.exe run --config examples\sim_default.ini --snr-db 18 --seed 99 --quiet
```

**`sweep`** — Legacy positionals: `ch mod eq [taps]`, or set **`--ch` / `--mod` / `--eq`**. Optional: **`--out` / `-o file.csv`** (use **`-`** for stdout), **`--frames`**, **`--seed`**, **`--snr-min`**, **`--snr-max`**, **`--snr-step`**. CSV columns: **`snr_db,es_n0_db,eb_n0_db,ber,errors,bits`**, with comment lines at the top (`# generated=…`, `# cmdline=…`).

**Plot (optional):** `pip install matplotlib`, then e.g. `python scripts/plot_ber.py build\my_sweep.csv --x eb_n0_db -o ber.png`.

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
