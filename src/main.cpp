#include "ofdm/self_test.hpp"
#include "ofdm/simulation.hpp"
#include "ofdm/types.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>

using namespace std;

static void print_usage(const char* argv0) {
    cerr << "Usage:\n"
         << "  " << argv0 << "              Interactive mode\n"
         << "  " << argv0 << " test         Run automated self-tests (exit 0 = pass)\n"
         << "  " << argv0 << " sweep <ch> <mod> <eq> [taps]\n"
         << "       ch: 1=AWGN  2=Rayleigh  3=ideal\n"
         << "       mod: 1=QPSK 2=16-QAM 3=64-QAM\n"
         << "       eq: 1=MMSE 2=ZF\n"
         << "       taps: CIR length for Rayleigh (default 4)\n";
}

static int interactive_main() {
    ofdm::SimParams p;
    mt19937 gen(random_device{}());

    cout << "=== OFDM System Configuration ===\n";
    cout << "Channel: 1=AWGN  2=Rayleigh (freq-selective)  3=Ideal\nChoice: ";
    cin >> p.channel;
    if (cin.fail() || p.channel < 1 || p.channel > 3) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        p.channel = 1;
    }

    cout << "Modulation: 1=QPSK  2=16-QAM  3=64-QAM\nChoice: ";
    int modc;
    cin >> modc;
    if (cin.fail() || modc < 1 || modc > 3) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        modc = 1;
    }
    p.scheme = (modc == 2) ? ofdm::ModScheme::QAM16
                           : (modc == 3) ? ofdm::ModScheme::QAM64 : ofdm::ModScheme::QPSK;

    cout << "Equalizer: 1=MMSE  2=ZF\nChoice: ";
    cin >> p.equalizer;
    if (cin.fail() || p.equalizer < 1 || p.equalizer > 2) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        p.equalizer = 1;
    }

    cout << "SNR (dB): ";
    cin >> p.snr_db;
    if (cin.fail()) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        p.snr_db = 20.0;
    }

    if (p.channel == 2) {
        cout << "Rayleigh taps L (CIR length): ";
        cin >> p.rayleigh_taps;
        if (cin.fail() || p.rayleigh_taps < 1) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            p.rayleigh_taps = 4;
        }
    }

    const char* ch_name = (p.channel == 1) ? "AWGN" : (p.channel == 2) ? "Rayleigh" : "Ideal";
    cout << "\n--- Parameters ---\n";
    cout << "N=" << p.N << ", CP=" << p.cp_len << ", pilot spacing=" << p.pilot_spacing
         << ", frames=" << p.num_frames << "\n";
    cout << "Modulation: " << ofdm::scheme_name(p.scheme) << "\n";
    cout << "Channel: " << ch_name << "  Equalizer: " << (p.equalizer == 2 ? "ZF" : "MMSE")
         << "  SNR=" << p.snr_db << " dB\n\n";

    ofdm::SimResult res = ofdm::run_ofdm_sim(p, gen);
    cout << fixed << setprecision(6);
    cout << "Frames processed: " << res.frames_processed << "/" << p.num_frames << "\n";
    cout << "BER: " << res.ber << "  (" << res.errors << " errors / " << res.bits_compared << " bits)\n";
    return 0;
}

static int sweep_main(int argc, char** argv) {
    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }
    ofdm::SimParams p;
    p.verbose = false;
    p.channel = atoi(argv[2]);
    int modc = atoi(argv[3]);
    p.equalizer = atoi(argv[4]);
    p.scheme = (modc == 2) ? ofdm::ModScheme::QAM16
                           : (modc == 3) ? ofdm::ModScheme::QAM64 : ofdm::ModScheme::QPSK;
    if (argc >= 6) p.rayleigh_taps = max(1, atoi(argv[5]));
    p.num_frames = 200;

    mt19937 gen(12345u);
    cout << "snr_db,ber,errors,bits\n";
    cout << fixed << setprecision(8);
    for (int s = 0; s <= 40; s += 2) {
        p.snr_db = static_cast<double>(s);
        ofdm::SimResult r = ofdm::run_ofdm_sim(p, gen);
        cout << s << "," << r.ber << "," << r.errors << "," << r.bits_compared << "\n";
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        string cmd = argv[1];
        if (cmd == "test" || cmd == "--test" || cmd == "-t")
            return ofdm::run_self_tests() == 0 ? 0 : 1;
        if (cmd == "sweep")
            return sweep_main(argc, argv);
        if (cmd == "-h" || cmd == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        cerr << "Unknown command: " << cmd << "\n";
        print_usage(argv[0]);
        return 1;
    }
    return interactive_main();
}
