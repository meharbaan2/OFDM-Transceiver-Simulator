#include "ofdm/cli.hpp"

#include "ofdm/modulation.hpp" // scheme_name, mod helpers

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ofdm::cli {

namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

ModScheme mod_from_choice(int m) {
    if (m == 2) return ModScheme::QAM16;
    if (m == 3) return ModScheme::QAM64;
    return ModScheme::QPSK;
}

bool parse_bool(const std::string& v, bool& out) {
    std::string t = to_lower(trim(v));
    if (t == "1" || t == "true" || t == "yes" || t == "on") {
        out = true;
        return true;
    }
    if (t == "0" || t == "false" || t == "no" || t == "off") {
        out = false;
        return true;
    }
    return false;
}

std::string utc_timestamp_local() {
    const auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

bool apply_key_value(const std::string& key_in, const std::string& val_in, SimParams& p, std::string& err) {
    std::string key = to_lower(trim(key_in));
    for (char& c : key)
        if (c == '-') c = '_';
    const std::string val = trim(val_in);
    try {
        if (key == "seed") {
            if (val.empty())
                p.seed = 0;
            else
                p.seed = static_cast<uint32_t>(std::stoul(val));
            return true;
        }
        if (val.empty()) {
            err = "empty value for key: " + key;
            return false;
        }
        if (key == "n" || key == "subcarriers" || key == "fft_size") {
            p.N = std::max(8, std::stoi(val));
        } else if (key == "cp_len" || key == "cp") {
            p.cp_len = std::max(0, std::stoi(val));
        } else if (key == "pilot_spacing") {
            p.pilot_spacing = std::max(1, std::stoi(val));
        } else if (key == "num_frames" || key == "frames") {
            p.num_frames = std::max(1, std::stoi(val));
        } else if (key == "channel") {
            p.channel = std::stoi(val);
        } else if (key == "modulation" || key == "mod") {
            p.scheme = mod_from_choice(std::stoi(val));
        } else if (key == "equalizer" || key == "eq") {
            p.equalizer = std::stoi(val);
        } else if (key == "snr_db" || key == "snr") {
            p.snr_db = std::stod(val);
        } else if (key == "rayleigh_taps" || key == "taps") {
            p.rayleigh_taps = std::max(1, std::stoi(val));
        } else if (key == "verbose") {
            if (!parse_bool(val, p.verbose)) {
                err = "verbose must be true/false";
                return false;
            }
        } else {
            err = "unknown key: " + key;
            return false;
        }
    } catch (...) {
        err = "bad value for key: " + key;
        return false;
    }
    return true;
}

bool split_kv_line(const std::string& line, std::string& key, std::string& val) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = trim(line.substr(0, eq));
    val = trim(line.substr(eq + 1));
    return !key.empty();
}

} // namespace

bool load_ini(const std::string& path, SimParams& p, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "cannot open config: " + path;
        return false;
    }
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        std::string key, val;
        if (!split_kv_line(line, key, val)) {
            err = path + ":" + std::to_string(lineno) + ": expected key=value";
            return false;
        }
        if (!apply_key_value(key, val, p, err)) {
            err = path + ":" + std::to_string(lineno) + ": " + err;
            return false;
        }
    }
    return true;
}

static bool set_flag(const std::string& key, const std::string& val, SimParams& p, std::string& err) {
    std::string k = to_lower(key);
    if (k.size() > 2 && k.substr(0, 2) == "--") k = k.substr(2);
    return apply_key_value(k, val, p, err);
}

bool parse_run_args(int argc, char** argv, SimParams& p, std::string& err) {
    p = SimParams{};
    std::vector<std::string> configs;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-c" || a == "--config") {
            if (i + 1 >= argc) {
                err = "--config needs a path";
                return false;
            }
            configs.push_back(argv[i + 1]);
            ++i;
        }
    }
    for (const auto& c : configs) {
        if (!load_ini(c, p, err)) return false;
    }

    for (int i = 2; i < argc; ) {
        std::string a = argv[i];
        if (a == "-c" || a == "--config") {
            i += 2;
            continue;
        }
        if (a == "--quiet" || a == "-q") {
            p.verbose = false;
            ++i;
            continue;
        }
        if (a == "--verbose" || a == "-v") {
            p.verbose = true;
            ++i;
            continue;
        }
        if (a.rfind("--", 0) != 0) {
            err = "unexpected argument: " + a;
            return false;
        }
        size_t eq = a.find('=');
        if (eq != std::string::npos) {
            std::string k = a.substr(0, eq);
            std::string v = a.substr(eq + 1);
            if (!set_flag(k, v, p, err)) return false;
            ++i;
            continue;
        }
        if (i + 1 >= argc) {
            err = "missing value for flag: " + a;
            return false;
        }
        if (!set_flag(a, argv[i + 1], p, err)) return false;
        i += 2;
    }
    return true;
}

bool parse_sweep_args(int argc, char** argv, SweepOptions& o, std::string& err) {
    o = SweepOptions{};
    o.p = SimParams{};
    o.p.verbose = false;
    o.p.num_frames = 200;
    o.p.channel = 0;
    o.p.equalizer = 0;
    o.p.scheme = ModScheme::QPSK;

    int i = 2;
    if (argc > i && argv[i][0] != '-') {
        if (argc < 5) {
            err = "sweep: need channel modulation equalizer (or use --ch --mod --eq)";
            return false;
        }
        o.p.channel = std::atoi(argv[i++]);
        const int modc = std::atoi(argv[i++]);
        o.p.equalizer = std::atoi(argv[i++]);
        o.p.scheme = mod_from_choice(modc);
        if (argc > i && argv[i][0] != '-') o.p.rayleigh_taps = std::max(1, std::atoi(argv[i++]));
    }

    while (i < argc) {
        std::string a = argv[i];
        auto take_val = [&]() -> const char* {
            if (i + 1 >= argc) return nullptr;
            ++i;
            return argv[i];
        };
        if (a == "-o" || a == "--out") {
            const char* v = take_val();
            if (!v) {
                err = "--out needs a path (use - for stdout)";
                return false;
            }
            o.out_path = v;
            ++i;
        } else if (a.rfind("--out=", 0) == 0) {
            o.out_path = a.substr(6);
            ++i;
        } else if (a == "--ch" || a == "--channel") {
            const char* v = take_val();
            if (!v) {
                err = "--ch needs value";
                return false;
            }
            o.p.channel = std::atoi(v);
            ++i;
        } else if (a.rfind("--ch=", 0) == 0 || a.rfind("--channel=", 0) == 0) {
            const size_t pos = a.find('=');
            o.p.channel = std::atoi(a.substr(pos + 1).c_str());
            ++i;
        } else if (a == "--mod" || a == "--modulation") {
            const char* v = take_val();
            if (!v) {
                err = "--mod needs value (1–3)";
                return false;
            }
            o.p.scheme = mod_from_choice(std::atoi(v));
            ++i;
        } else if (a.rfind("--mod=", 0) == 0) {
            o.p.scheme = mod_from_choice(std::atoi(a.substr(6).c_str()));
            ++i;
        } else if (a == "--eq" || a == "--equalizer") {
            const char* v = take_val();
            if (!v) {
                err = "--eq needs value";
                return false;
            }
            o.p.equalizer = std::atoi(v);
            ++i;
        } else if (a.rfind("--eq=", 0) == 0) {
            o.p.equalizer = std::atoi(a.substr(5).c_str());
            ++i;
        } else if (a == "--taps" || a == "--rayleigh-taps") {
            const char* v = take_val();
            if (!v) {
                err = "--taps needs value";
                return false;
            }
            o.p.rayleigh_taps = std::max(1, std::atoi(v));
            ++i;
        } else if (a.rfind("--taps=", 0) == 0) {
            o.p.rayleigh_taps = std::max(1, std::atoi(a.substr(7).c_str()));
            ++i;
        } else if (a == "--frames" || a == "--num-frames") {
            const char* v = take_val();
            if (!v) {
                err = "--frames needs value";
                return false;
            }
            o.p.num_frames = std::max(1, std::atoi(v));
            ++i;
        } else if (a.rfind("--frames=", 0) == 0) {
            o.p.num_frames = std::max(1, std::atoi(a.substr(9).c_str()));
            ++i;
        } else if (a == "--seed") {
            const char* v = take_val();
            if (!v) {
                err = "--seed needs value";
                return false;
            }
            o.seed = static_cast<uint32_t>(std::strtoul(v, nullptr, 10));
            ++i;
        } else if (a.rfind("--seed=", 0) == 0) {
            o.seed = static_cast<uint32_t>(std::strtoul(a.substr(7).c_str(), nullptr, 10));
            ++i;
        } else if (a == "--snr-min") {
            const char* v = take_val();
            if (!v) {
                err = "--snr-min needs value";
                return false;
            }
            o.snr_min = std::atoi(v);
            ++i;
        } else if (a.rfind("--snr-min=", 0) == 0) {
            o.snr_min = std::atoi(a.substr(10).c_str());
            ++i;
        } else if (a == "--snr-max") {
            const char* v = take_val();
            if (!v) {
                err = "--snr-max needs value";
                return false;
            }
            o.snr_max = std::atoi(v);
            ++i;
        } else if (a.rfind("--snr-max=", 0) == 0) {
            o.snr_max = std::atoi(a.substr(10).c_str());
            ++i;
        } else if (a == "--snr-step") {
            const char* v = take_val();
            if (!v) {
                err = "--snr-step needs value";
                return false;
            }
            o.snr_step = std::max(1, std::atoi(v));
            ++i;
        } else if (a.rfind("--snr-step=", 0) == 0) {
            o.snr_step = std::max(1, std::atoi(a.substr(11).c_str()));
            ++i;
        } else {
            err = "unknown sweep flag: " + a;
            return false;
        }
    }

    if (o.p.channel < 1 || o.p.channel > 3) {
        err = "sweep: channel must be 1–3 (AWGN / Rayleigh / ideal)";
        return false;
    }
    if (o.p.equalizer < 1 || o.p.equalizer > 2) {
        err = "sweep: equalizer must be 1 (MMSE) or 2 (ZF)";
        return false;
    }
    if (o.snr_min > o.snr_max) {
        err = "sweep: snr-min must be <= snr-max";
        return false;
    }
    return true;
}

void write_sweep_file_header(std::ostream& os, const SweepOptions& o, int argc, char** argv) {
    os << "# OFDMSim sweep\n";
    os << "# generated=" << utc_timestamp_local() << "\n";
    os << "# cmdline=";
    for (int k = 0; k < argc; ++k)
        os << (k ? " " : "") << argv[k];
    os << "\n";
    os << "# channel=" << o.p.channel << " modulation=" << scheme_name(o.p.scheme)
       << " equalizer=" << o.p.equalizer << " frames=" << o.p.num_frames << " seed=" << o.seed << "\n";
    os << "# snr_range=" << o.snr_min << ":" << o.snr_step << ":" << o.snr_max << "\n";
    os << "snr_db,es_n0_db,eb_n0_db,ber,errors,bits\n";
}

} // namespace ofdm::cli
