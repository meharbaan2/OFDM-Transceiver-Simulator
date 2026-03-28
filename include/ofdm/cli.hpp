#pragma once

#include "ofdm/simulation.hpp"

#include <ostream>
#include <string>

namespace ofdm::cli {

// key=value INI-style file (lines like N=64, # comments). Merges into p (overwrites).
bool load_ini(const std::string& path, SimParams& p, std::string& err);

// argv[0]=exe, argv[1]=run, argv[2..]=flags. Supports --config first, then overrides.
bool parse_run_args(int argc, char** argv, SimParams& p, std::string& err);

struct SweepOptions {
    SimParams p;
    int snr_min = 0;
    int snr_max = 40;
    int snr_step = 2;
    uint32_t seed = 12345;
    std::string out_path; // empty = stdout; "-" = stdout
};

// argv[0]=exe, argv[1]=sweep. Legacy: sweep <ch> <mod> <eq> [taps] [optional --flags...]
bool parse_sweep_args(int argc, char** argv, SweepOptions& o, std::string& err);

void write_sweep_file_header(std::ostream& os, const SweepOptions& o, int argc, char** argv);

} // namespace ofdm::cli
