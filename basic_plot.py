#!/usr/bin/env python3
"""
basic_plot.py

Create four basic visualization figures and save them to test_results/:

1. Matrix runtime vs n
2. Matrix speedup vs n
3. Sum runtime vs n
4. Sum speedup vs n

Features:
- English labels/titles suitable for reports.
- Value annotations on points with alternating offsets to reduce overlap.
- Robust parsing for sum_results.csv even when it mixes two layouts:
    Layout A (16 fields):
        n,repeat,warmup,naive_ms,2way_ms,4way_ms,recursive_ms,iterative_ms,...
    Layout B (20 fields):
        n,repeat_naive,repeat_2way,repeat_4way,repeat_recursive,repeat_iterative,warmup,
        naive_ms,2way_ms,4way_ms,recursive_ms,iterative_ms,...
- If duplicate n values exist in sum_results.csv, the last occurrence is kept.

Usage:
    python basic_plot.py
    python basic_plot.py --base-dir .
    python basic_plot.py --matrix-results /path/to/matrix_results.csv --sum-results /path/to/sum_results.csv
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Iterable, Sequence

import matplotlib.pyplot as plt
import pandas as pd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create basic plots for matrix/sum experiment results.")
    parser.add_argument("--base-dir", type=Path, default=Path("."), help="Base directory containing the CSV files.")
    parser.add_argument("--matrix-results", type=Path, default=None, help="Path to matrix_results.csv")
    parser.add_argument("--sum-results", type=Path, default=None, help="Path to sum_results.csv")
    parser.add_argument("--out-dir", type=Path, default=Path("test_results"), help="Output directory for figures")
    return parser.parse_args()


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def read_matrix_results(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    expected = {"n", "naive_time_ms", "cache_time_ms", "speedup"}
    missing = expected - set(df.columns)
    if missing:
        raise ValueError(f"{path} is missing columns: {sorted(missing)}")
    return df.sort_values("n").reset_index(drop=True)


def _to_float(text: str) -> float:
    return float(str(text).strip())


def read_sum_results(path: Path) -> pd.DataFrame:
    """
    Parse sum_results.csv robustly even if the file contains mixed row layouts.
    Keeps the last occurrence for duplicate n.
    """
    rows_out: list[dict] = []

    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)  # ignore header text; rows may not match it
        for raw in reader:
            if not raw:
                continue
            row = [cell.strip() for cell in raw]
            if all(cell == "" for cell in row):
                continue

            # Layout A: 16 fields
            if len(row) == 16:
                rows_out.append(
                    {
                        "n": _to_float(row[0]),
                        "naive_ms": _to_float(row[3]),
                        "2way_ms": _to_float(row[4]),
                        "4way_ms": _to_float(row[5]),
                        "recursive_ms": _to_float(row[6]),
                        "iterative_ms": _to_float(row[7]),
                        "speedup_2way": _to_float(row[8]),
                        "speedup_4way": _to_float(row[9]),
                        "speedup_recursive": _to_float(row[10]),
                        "speedup_iterative": _to_float(row[11]),
                    }
                )
            # Layout B: 20 fields
            elif len(row) == 20:
                rows_out.append(
                    {
                        "n": _to_float(row[0]),
                        "naive_ms": _to_float(row[7]),
                        "2way_ms": _to_float(row[8]),
                        "4way_ms": _to_float(row[9]),
                        "recursive_ms": _to_float(row[10]),
                        "iterative_ms": _to_float(row[11]),
                        "speedup_2way": _to_float(row[12]),
                        "speedup_4way": _to_float(row[13]),
                        "speedup_recursive": _to_float(row[14]),
                        "speedup_iterative": _to_float(row[15]),
                    }
                )
            else:
                # Skip malformed rows silently; better than failing on a mixed export.
                continue

    if not rows_out:
        raise ValueError(f"No valid data rows parsed from {path}")

    df = pd.DataFrame(rows_out)
    # Keep the last occurrence for duplicate n, which is usually the newer export block.
    df = df.drop_duplicates(subset=["n"], keep="last")
    df["n"] = df["n"].astype(int)
    return df.sort_values("n").reset_index(drop=True)


def save_figure(fig: plt.Figure, out_path: Path) -> None:
    fig.tight_layout()
    fig.savefig(out_path, dpi=220, bbox_inches="tight")
    plt.close(fig)


def format_ms(v: float) -> str:
    if v < 0.01:
        return f"{v:.4f}"
    if v < 0.1:
        return f"{v:.3f}"
    if v < 1:
        return f"{v:.3f}"
    if v < 10:
        return f"{v:.2f}"
    return f"{v:.1f}"


def format_ratio(v: float) -> str:
    return f"{v:.2f}x"


def annotate_series(
    ax: plt.Axes,
    x: Sequence[float],
    y: Sequence[float],
    formatter,
    offsets: Sequence[tuple[int, int]],
    fontsize: int = 8,
) -> None:
    """
    Add value labels with alternating offsets to reduce overlap.
    """
    for i, (xi, yi) in enumerate(zip(x, y)):
        dx, dy = offsets[i % len(offsets)]
        ax.annotate(
            formatter(float(yi)),
            xy=(xi, yi),
            xytext=(dx, dy),
            textcoords="offset points",
            fontsize=fontsize,
            ha="center",
            va="bottom" if dy >= 0 else "top",
            clip_on=False,
        )


def plot_matrix_runtime(df: pd.DataFrame, out_dir: Path) -> None:
    fig, ax = plt.subplots(figsize=(9, 5.6))

    x = df["n"]
    naive = df["naive_time_ms"]
    cache = df["cache_time_ms"]

    ax.plot(x, naive, marker="o", label="Naive")
    ax.plot(x, cache, marker="o", label="Cache-optimized")

    # x4 natural growth reference: for matrix O(n^2), doubling n tends to ~x4 runtime.
    n0 = float(x.iloc[0])
    y0 = float(naive.iloc[0])
    ref = y0 * (x / n0) ** 2
    ax.plot(x, ref, linestyle="--", label="x4 growth reference")

    annotate_series(ax, x.tolist(), naive.tolist(), format_ms, [(0, 8), (0, -10)])
    annotate_series(ax, x.tolist(), cache.tolist(), format_ms, [(10, 8), (10, -10)])

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xlabel("Problem size n")
    ax.set_ylabel("Runtime (ms)")
    ax.set_title("Matrix runtime vs n")
    ax.legend()
    ax.grid(True, which="both", alpha=0.3)

    save_figure(fig, out_dir / "matrix_runtime_vs_n.png")


def plot_matrix_speedup(df: pd.DataFrame, out_dir: Path) -> None:
    fig, ax = plt.subplots(figsize=(9, 5.2))

    x = df["n"]
    y = df["speedup"]

    ax.plot(x, y, marker="o", label="Speedup")
    annotate_series(ax, x.tolist(), y.tolist(), format_ratio, [(0, 8), (0, -10)])

    ax.set_xscale("log", base=2)
    ax.set_xlabel("Problem size n")
    ax.set_ylabel("Speedup (naive / cache-optimized)")
    ax.set_title("Matrix speedup vs n")
    ax.grid(True, which="both", alpha=0.3)

    save_figure(fig, out_dir / "matrix_speedup_vs_n.png")


def plot_sum_runtime(df: pd.DataFrame, out_dir: Path) -> None:
    fig, ax = plt.subplots(figsize=(10, 6))

    x = df["n"]

    series = [
        ("Naive", df["naive_ms"], [(0, 8), (0, -10)]),
        ("2-way", df["2way_ms"], [(10, 8), (10, -10)]),
        ("4-way", df["4way_ms"], [(-10, 8), (-10, -10)]),
        ("Recursive", df["recursive_ms"], [(16, 8), (16, -10)]),
        ("Iterative", df["iterative_ms"], [(-16, 8), (-16, -10)]),
    ]

    for label, y, offsets in series:
        ax.plot(x, y, marker="o", label=label)
        annotate_series(ax, x.tolist(), y.tolist(), format_ms, offsets, fontsize=7)

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xlabel("Problem size n")
    ax.set_ylabel("Runtime (ms)")
    ax.set_title("Sum runtime vs n")
    ax.legend(ncol=2)
    ax.grid(True, which="both", alpha=0.3)

    save_figure(fig, out_dir / "sum_runtime_vs_n.png")


def plot_sum_speedup(df: pd.DataFrame, out_dir: Path) -> None:
    fig, ax = plt.subplots(figsize=(10, 6))

    x = df["n"]

    series = [
        ("2-way", df["speedup_2way"], [(10, 8), (10, -10)]),
        ("4-way", df["speedup_4way"], [(-10, 8), (-10, -10)]),
        ("Recursive", df["speedup_recursive"], [(16, 8), (16, -10)]),
        ("Iterative", df["speedup_iterative"], [(-16, 8), (-16, -10)]),
    ]

    for label, y, offsets in series:
        ax.plot(x, y, marker="o", label=label)
        annotate_series(ax, x.tolist(), y.tolist(), format_ratio, offsets, fontsize=7)

    ax.set_xscale("log", base=2)
    ax.set_xlabel("Problem size n")
    ax.set_ylabel("Speedup vs naive")
    ax.set_title("Sum speedup vs n")
    ax.legend(ncol=2)
    ax.grid(True, which="both", alpha=0.3)

    save_figure(fig, out_dir / "sum_speedup_vs_n.png")


def main() -> None:
    args = parse_args()

    matrix_results = args.matrix_results or (args.base_dir / "matrix_results.csv")
    sum_results = args.sum_results or (args.base_dir / "sum_results.csv")
    out_dir = args.out_dir

    ensure_dir(out_dir)

    matrix_df = read_matrix_results(matrix_results)
    sum_df = read_sum_results(sum_results)

    plot_matrix_runtime(matrix_df, out_dir)
    plot_matrix_speedup(matrix_df, out_dir)
    plot_sum_runtime(sum_df, out_dir)
    plot_sum_speedup(sum_df, out_dir)

    print(f"Saved figures to: {out_dir.resolve()}")


if __name__ == "__main__":
    main()
