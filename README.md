# 🛰️ OFDM Wireless Communication System Simulator (C++)

### 🔬 Overview  
This project is a **fully functional Orthogonal Frequency Division Multiplexing (OFDM)** communication system simulator implemented in modern **C++**.  
It models the **transmitter, channel, and receiver chain** of a real digital wireless system — including modulation, channel effects, equalization, and bit error rate (BER) analysis.  

The simulator allows you to test system performance under various conditions such as:
- **AWGN** (Additive White Gaussian Noise)
- **Rayleigh multipath fading**
- **Different modulation schemes** (QPSK, 16-QAM, 64-QAM)
- **Multiple equalizers** (MMSE, Zero Forcing, Simple)

It’s a complete **baseband link-level simulator** — the same type of model used in research and pre-hardware validation of Wi-Fi, LTE, and 5G systems.

---

## 🧠 Features

OFDM modulation/demodulation (IFFT/FFT based)  
Pilot-based channel estimation  
AWGN and Rayleigh fading channel models  
MMSE, ZF, and Simple equalizers  
Dynamic configuration via user input  
BER and SNR computation  
Detailed per-frame debugging and signal power analysis  

---

## 🧩 System Model
[Bit Source]
->
[Modulation: QPSK / QAM]
->
[OFDM Modulation + Pilot Insertion]
->
[Channel: AWGN / Rayleigh Fading / Ideal]
->
[OFDM Demodulation + Channel Estimation]
->
[Equalization (MMSE / ZF / Simple)]
->
[Demodulation + BER Computation]

---

## How It Works (Conceptually)
The simulator follows real-world signal processing principles:

Radio transmitter encodes data onto subcarriers
Noise & multipath affect the signal	              
Receiver FFT recovers subcarriers	
Equalizer compensates for fading
Decoder recovers bits and measures BER

This allows you to test link-level performance of OFDM under controlled virtual conditions — identical to the methods used in 5G/6G PHY research.

---

## 🧪 Example Output
=== OFDM System Configuration ===
Select Channel Model:
1. AWGN (Additive White Gaussian Noise)
2. Rayleigh Fading (Multipath)
3. No Channel (Ideal - for testing)
Choice (1-3): 1

Select Modulation Scheme:
1. QPSK (2 bits/symbol)
2. 16-QAM (4 bits/symbol)
3. 64-QAM (6 bits/symbol)
Choice (1-3): 2

Select Equalizer Type:
1. MMSE (Minimum Mean Square Error)
2. ZF (Zero Forcing)
3. Simple Equalizer
Choice (1-3): 1

Enter SNR (dB): 20

=== OFDM System Parameters ===
Subcarriers: 64, CP: 16
Pilots per frame: 8, Data per frame: 56
Number of frames: 100
Total data symbols: 5600
Total bits: 22400
Modulation: 16-QAM
Channel: AWGN
SNR: 20 dB

=== Transmitter ===
Data symbols: 5600
Symbols with pilots: 6400
Transmitted signal length: 8000

=== Channel: AWGN ===
Received signal length: 8000

=== Receiver ===
Received symbols with pilots: 6400

=== SIGNAL POWER ANALYSIS ===
Data symbols power: 0.998143
TX signal power: 0.997411
RX signal power: 1.01
RX symbols power: 1.01017
Channel gain: 1.01205

=== Channel State Information (Frame 0) ===
Channel magnitude statistics:
  Min: 0.915944
  10%: 0.92706
  Median: 1.02975
  90%: 1.07874
  Max: 1.08906
Weak subcarriers (|H| < 0.1): 0/64

=== Equalization Debug (Frame 0) ===
Equalizer: MMSE
Equalized frame power: 0.948247
First 5 equalized symbols:
  (1.02611, 0.000182768)
  (0.866264, -0.854319)
  (1.03565, -0.332461)
  (0.225322, -0.371625)
  (-0.344421, 0.975701)

=== System Performance Summary ===
Frames processed: 100 out of 100
Equalized data symbols: 5600
Received bits: 22400
Constellation-based SNR estimation:
Estimated SNR: 60.467 linear, 17.8152 dB

=== Results ===
Modulation: 16-QAM
Channel: AWGN
Equalizer: MMSE
Target SNR: 20 dB
BER: 4.46429e-05
Bits compared: 22400 out of 22400
Errors: 1

---

### 🧰 Requirements
- C++17 or higher
- FFTW or equivalent FFT library
- Any C++ compiler (GCC, Clang, MSVC)
