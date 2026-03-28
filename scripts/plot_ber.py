#!/usr/bin/env python3
"""
Plot BER vs SNR or Eb/N0 from OFDMSim sweep CSV (lines starting with # are skipped).

  pip install matplotlib
  python scripts/plot_ber.py sweep.csv --x eb_n0
  python scripts/plot_ber.py sweep.csv --x snr -o ber.png
"""

import argparse
import csv
import sys
from pathlib import Path
from typing import Dict, List, Tuple


def read_rows(path: Path) -> Tuple[List[str], List[Dict[str, str]]]:
    with path.open(newline="", encoding="utf-8") as f:
        lines = []
        for line in f:
            if line.startswith("#"):
                continue
            lines.append(line)
    reader = csv.DictReader(lines)
    if not reader.fieldnames:
        raise SystemExit("CSV has no header row")
    rows = list(reader)
    return reader.fieldnames, rows


def main() -> int:
    ap = argparse.ArgumentParser(description="Plot BER from OFDMSim sweep CSV")
    ap.add_argument("csv_file", type=Path)
    ap.add_argument("--x", choices=("snr_db", "eb_n0_db", "es_n0_db"), default="eb_n0_db")
    ap.add_argument("-o", "--output", type=Path, help="Save figure (default: show)")
    args = ap.parse_args()

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("Install matplotlib:  pip install matplotlib", file=sys.stderr)
        return 1

    _, rows = read_rows(args.csv_file)
    if not rows:
        print("No data rows", file=sys.stderr)
        return 1

    xk = args.x
    if xk not in rows[0]:
        print(f"Column {xk!r} not in CSV. Have: {list(rows[0].keys())}", file=sys.stderr)
        return 1

    xs = [float(r[xk]) for r in rows]
    bers = [float(r["ber"]) for r in rows]

    fig, ax = plt.subplots(figsize=(7, 4.5))
    ax.semilogy(xs, bers, "o-", linewidth=1.5, markersize=5)
    ax.set_xlabel(xk.replace("_", " ") + " (dB)")
    ax.set_ylabel("BER")
    ax.grid(True, which="both", ls=":", alpha=0.5)
    ax.set_title(args.csv_file.name)
    fig.tight_layout()

    if args.output:
        fig.savefig(args.output, dpi=150)
        print(f"Wrote {args.output}")
    else:
        plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
