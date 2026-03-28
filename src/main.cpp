#include "ofdm/cli.hpp"
#include "ofdm/modulation.hpp"
#include "ofdm/self_test.hpp"
#include "ofdm/simulation.hpp"
#include "ofdm/types.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using namespace std;

static void print_usage(const char* argv0) {
    cerr << "Usage:\n"
         << "  " << argv0 << "                    Interactive mode\n"
         << "  " << argv0 << " test                  Self-tests (exit 0 = pass)\n"
         << "  " << argv0 << " run [flags]           One simulation (see below)\n"
         << "  " << argv0 << " sweep <ch> <mod> <eq> [taps] [options]\n"
         << "  " << argv0 << " sweep --ch N --mod N --eq N ...\n"
         << "  " << argv0 << " --help\n\n"
         << "run:\n"
         << "  --config, -c <file>     INI key=value (repeatable; later flags override)\n"
         << "  --channel, --ch         1=AWGN  2=Rayleigh  3=ideal\n"
         << "  --mod, --modulation     1=QPSK  2=16-QAM  3=64-QAM\n"
         << "  --eq, --equalizer      1=MMSE  2=ZF\n"
         << "  --snr, --snr-db       SNR (dB), treated as Es/N0 for Eb/N0 reporting\n"
         << "  --frames, --num-frames number of OFDM symbols (frames)\n"
         << "  --seed                 RNG seed (0 = random)\n"
         << "  --n, --subcarriers     FFT size N\n"
         << "  --cp, --cp-len         cyclic prefix length\n"
         << "  --pilot-spacing         pilot comb spacing\n"
         << "  --taps, --rayleigh-taps CIR length (Rayleigh)\n"
         << "  --quiet, -q / --verbose, -v\n\n"
         << "sweep options:\n"
         << "  --out, -o <file>       write CSV (default: stdout; use - for stdout)\n"
         << "  --frames, --seed\n"
         << "  --snr-min, --snr-max, --snr-step (default 0, 40, 2)\n";
}

static int interactive_main() {
    ofdm::SimParams p;
    auto gen = ofdm::make_rng(0);

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
         << "  SNR=" << p.snr_db << " dB (Es/N0)\n";
    cout << "Eb/N0 ≈ " << fixed << setprecision(2) << ofdm::eb_n0_db(p.snr_db, p.scheme) << " dB\n\n";

    ofdm::SimResult res = ofdm::run_ofdm_sim(p, gen);
    cout << fixed << setprecision(6);
    cout << "Frames processed: " << res.frames_processed << "/" << p.num_frames << "\n";
    cout << "BER: " << res.ber << "  (" << res.errors << " errors / " << res.bits_compared << " bits)\n";
    return 0;
}

static int run_main(int argc, char** argv) {
    ofdm::SimParams p;
    string err;
    if (!ofdm::cli::parse_run_args(argc, argv, p, err)) {
        cerr << err << "\n";
        print_usage(argv[0]);
        return 1;
    }
    auto gen = ofdm::make_rng(p.seed);
    ofdm::SimResult res = ofdm::run_ofdm_sim(p, gen);
    if (p.verbose) {
        cout << fixed << setprecision(6);
        cout << "Es/N0=" << res.es_n0_db << " dB  Eb/N0=" << res.eb_n0_db << " dB\n";
        cout << "Frames: " << res.frames_processed << "/" << p.num_frames
             << "  BER: " << res.ber << "  errors: " << res.errors << " / " << res.bits_compared << " bits\n";
    } else {
        cout << fixed << setprecision(8);
        cout << res.ber << " " << res.errors << " " << res.bits_compared << " " << res.es_n0_db << " "
             << res.eb_n0_db << "\n";
    }
    return 0;
}

static int sweep_main(int argc, char** argv) {
    ofdm::cli::SweepOptions opt;
    string err;
    if (!ofdm::cli::parse_sweep_args(argc, argv, opt, err)) {
        cerr << err << "\n";
        print_usage(argv[0]);
        return 1;
    }

    ostream* out = &cout;
    ofstream fout;
    if (!opt.out_path.empty() && opt.out_path != "-") {
        fout.open(opt.out_path, ios::out | ios::trunc);
        if (!fout) {
            cerr << "cannot open output: " << opt.out_path << "\n";
            return 1;
        }
        out = &fout;
    }

    ofdm::cli::write_sweep_file_header(*out, opt, argc, argv);
    *out << fixed << setprecision(8);

    mt19937 gen(opt.seed);
    for (int s = opt.snr_min; s <= opt.snr_max; s += opt.snr_step) {
        opt.p.snr_db = static_cast<double>(s);
        ofdm::SimResult r = ofdm::run_ofdm_sim(opt.p, gen);
        *out << s << "," << r.es_n0_db << "," << r.eb_n0_db << "," << r.ber << "," << r.errors << ","
             << r.bits_compared << "\n";
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        string cmd = argv[1];
        if (cmd == "test" || cmd == "--test" || cmd == "-t")
            return ofdm::run_self_tests() == 0 ? 0 : 1;
        if (cmd == "run")
            return run_main(argc, argv);
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
