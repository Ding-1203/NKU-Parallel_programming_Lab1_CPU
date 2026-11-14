#!/usr/bin/env python3
"""
advanced_plot.py

Advanced visualization for profiling-derived CSV files.

Figures created:
1. Matrix profiler metric comparison (2x2)
2. Matrix performance-cause scatter figure
3. Sum profiler metric comparison by problem size (3 subplots)
4. Sum top-down 100% stacked bars for small/medium/large n (3 subplots)

Input files:
- matrix_summary.csv
- sum_summary.csv

Output directory:
- test_results/
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Sequence

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create advanced profiling plots.")
    parser.add_argument("--base-dir", type=Path, default=Path("."), help="Base directory containing CSV files.")
    parser.add_argument("--matrix-summary", type=Path, default=None, help="Path to matrix_summary.csv")
    parser.add_argument("--sum-summary", type=Path, default=None, help="Path to sum_summary.csv")
    parser.add_argument("--out-dir", type=Path, default=Path("test_results"), help="Output directory for figures")
    return parser.parse_args()


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def _normalize_columns(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df.columns = [str(c).strip().lower() for c in df.columns]
    return df


def _pick_first_existing(df: pd.DataFrame, candidates: Sequence[str], required: bool = False) -> str | None:
    for c in candidates:
        if c in df.columns:
            return c
    if required:
        raise ValueError(f"Missing required columns. Tried: {candidates}")
    return None


def read_matrix_summary(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    df = _normalize_columns(df)

    rename_map = {}
    mapping_candidates = {
        "n": ["n"],
        "mode": ["mode"],
        "status": ["status"],
        "program_time_ms": ["program_time_ms", "time_ms", "runtime_ms"],
        "ipc": ["ipc"],
        "cpi": ["cpi"],
        "l2_miss_pti": ["l2_miss_pti", "l2miss_pti", "l2_miss"],
        "backend_bound_memory": ["backend_bound_memory", "backend_bound.memory", "backend_memory"],
        "retiring": ["retiring"],
    }
    for target, candidates in mapping_candidates.items():
        col = _pick_first_existing(df, candidates, required=target in {"n", "mode", "program_time_ms"})
        if col is not None:
            rename_map[col] = target

    df = df.rename(columns=rename_map)

    numeric_cols = ["n", "program_time_ms", "ipc", "cpi", "l2_miss_pti", "backend_bound_memory", "retiring"]
    for c in numeric_cols:
        if c in df.columns:
            df[c] = pd.to_numeric(df[c], errors="coerce")

    if "status" in df.columns:
        df = df[df["status"].astype(str).str.upper().eq("OK") | df["status"].isna()]

    df = df.dropna(subset=["n", "mode", "program_time_ms"])
    df["n"] = df["n"].astype(int)
    df["mode"] = df["mode"].astype(str)
    return df.sort_values(["n", "mode"]).reset_index(drop=True)


def read_sum_summary(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    df = _normalize_columns(df)

    rename_map = {}
    mapping_candidates = {
        "n": ["n"],
        "mode": ["mode"],
        "status": ["status"],
        "program_time_ms": ["program_time_ms", "time_ms", "runtime_ms"],
        "ipc": ["ipc"],
        "cpi": ["cpi"],
        "frontend_bound": ["frontend_bound"],
        "bad_speculation": ["bad_speculation"],
        "backend_bound": ["backend_bound"],
        "backend_bound_memory": ["backend_bound_memory", "backend_bound.memory", "backend_memory"],
        "backend_bound_cpu": ["backend_bound_cpu", "backend_bound.cpu", "backend_cpu"],
        "retiring": ["retiring"],
    }
    for target, candidates in mapping_candidates.items():
        col = _pick_first_existing(df, candidates, required=target in {"n", "mode", "program_time_ms"})
        if col is not None:
            rename_map[col] = target

    df = df.rename(columns=rename_map)

    numeric_cols = [
        "n", "program_time_ms", "ipc", "cpi", "frontend_bound", "bad_speculation",
        "backend_bound", "backend_bound_memory", "backend_bound_cpu", "retiring"
    ]
    for c in numeric_cols:
        if c in df.columns:
            df[c] = pd.to_numeric(df[c], errors="coerce")

    if "status" in df.columns:
        df = df[df["status"].astype(str).str.upper().eq("OK") | df["status"].isna()]

    df = df.dropna(subset=["n", "mode"])
    df["n"] = df["n"].astype(int)
    df["mode"] = df["mode"].astype(str)
    return df.sort_values(["n", "mode"]).reset_index(drop=True)


def save_figure(fig: plt.Figure, out_path: Path) -> None:
    fig.tight_layout()
    fig.savefig(out_path, dpi=220, bbox_inches="tight")
    plt.close(fig)


def mode_style(mode: str) -> dict:
    m = mode.lower()
    if "naive" in m:
        return {"marker": "o", "linestyle": "-"}
    if "cache" in m:
        return {"marker": "s", "linestyle": "-"}
    if "2way" in m:
        return {"marker": "^", "linestyle": "-"}
    if "4way" in m:
        return {"marker": "D", "linestyle": "-"}
    if "recursive" in m:
        return {"marker": "P", "linestyle": "-"}
    if "iterative" in m:
        return {"marker": "X", "linestyle": "-"}
    return {"marker": "o", "linestyle": "-"}


def plot_matrix_profiler_compare(df: pd.DataFrame, out_dir: Path) -> None:
    metrics = [
        ("ipc", "IPC"),
        ("l2_miss_pti", "L2 Miss (pti)"),
        ("backend_bound_memory", "Memory-bound (%)"),
        ("retiring", "Retiring (%)"),
    ]

    fig, axes = plt.subplots(2, 2, figsize=(11, 8))
    axes = axes.ravel()

    for ax, (col, title) in zip(axes, metrics):
        if col not in df.columns:
            ax.set_visible(False)
            continue
        for mode in sorted(df["mode"].unique()):
            part = df[df["mode"] == mode].sort_values("n")
            if part[col].notna().sum() == 0:
                continue
            style = mode_style(mode)
            ax.plot(part["n"], part[col], label=mode, **style)
        ax.set_xscale("log", base=2)
        ax.set_xlabel("Problem size n")
        ax.set_ylabel(title)
        ax.set_title(title)
        ax.grid(True, which="both", alpha=0.3)

    handles, labels = axes[0].get_legend_handles_labels()
    if handles:
        fig.legend(handles, labels, loc="upper center", ncol=2, bbox_to_anchor=(0.5, 0.98))
    fig.suptitle("Matrix profiler metric comparison", y=1.03, fontsize=14)

    save_figure(fig, out_dir / "matrix_profiler_compare.png")


def plot_matrix_performance_cause(df: pd.DataFrame, out_dir: Path) -> None:
    panels = [
        ("ipc", "IPC"),
        ("l2_miss_pti", "L2 Miss (pti)"),
        ("backend_bound_memory", "Memory-bound (%)"),
    ]

    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))

    for ax, (col, xlab) in zip(axes, panels):
        if col not in df.columns or df[col].notna().sum() == 0:
            ax.set_visible(False)
            continue

        for mode in sorted(df["mode"].unique()):
            part = df[df["mode"] == mode].dropna(subset=[col, "program_time_ms"]).sort_values("n")
            if part.empty:
                continue
            style = mode_style(mode)
            ax.scatter(part[col], part["program_time_ms"], label=mode, marker=style["marker"])
        ax.set_ylabel("Runtime (ms)")
        ax.set_xlabel(xlab)
        ax.set_title(f"Runtime vs {xlab}")
        ax.grid(True, alpha=0.3)

    handles, labels = axes[0].get_legend_handles_labels()
    if handles:
        fig.legend(handles, labels, loc="upper center", ncol=2, bbox_to_anchor=(0.5, 1.02))
    fig.suptitle("Matrix performance-cause association", y=1.08, fontsize=14)

    save_figure(fig, out_dir / "matrix_performance_cause.png")


def choose_three_representative_ns(df: pd.DataFrame) -> list[int]:
    preferred_modes = {"naive", "2way", "4way", "recursive", "iterative"}
    mode_sets = df.groupby("n")["mode"].apply(lambda s: set(str(x).lower() for x in s))
    valid_ns = sorted(int(n) for n, modes in mode_sets.items() if preferred_modes.issubset(modes))
    if not valid_ns:
        valid_ns = sorted(df["n"].unique().tolist())
    if len(valid_ns) <= 3:
        return valid_ns
    return [valid_ns[0], valid_ns[len(valid_ns)//2], valid_ns[-1]]


def plot_sum_profiler_compare(df: pd.DataFrame, out_dir: Path) -> None:
    metrics = [
        ("ipc", "IPC"),
        ("backend_bound", "Backend_Bound (%)"),
        ("retiring", "Retiring (%)"),
    ]

    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))
    mode_order = ["naive", "2way", "4way", "recursive", "iterative"]

    for ax, (col, title) in zip(axes, metrics):
        if col not in df.columns or df[col].notna().sum() == 0:
            ax.set_visible(False)
            continue

        for mode in mode_order:
            part = df[df["mode"].str.lower() == mode].sort_values("n")
            if part.empty or part[col].notna().sum() == 0:
                continue
            style = mode_style(mode)
            ax.plot(part["n"], part[col], label=mode, **style)

        ax.set_xscale("log", base=2)
        ax.set_xlabel("Problem size n")
        ax.set_ylabel(title)
        ax.set_title(title)
        ax.grid(True, which="both", alpha=0.3)

    handles, labels = axes[0].get_legend_handles_labels()
    if handles:
        fig.legend(handles, labels, loc="upper center", ncol=3, bbox_to_anchor=(0.5, 1.02))
    fig.suptitle("Sum profiler metric comparison", y=1.10, fontsize=14)

    save_figure(fig, out_dir / "sum_profiler_compare.png")


def plot_sum_topdown_stacked(df: pd.DataFrame, out_dir: Path) -> None:
    rep_ns = choose_three_representative_ns(df)
    mode_order = ["naive", "2way", "4way", "recursive", "iterative"]
    required_cols = ["frontend_bound", "bad_speculation", "backend_bound", "retiring"]

    fig, axes = plt.subplots(1, len(rep_ns), figsize=(5.5 * len(rep_ns), 5.5))
    if len(rep_ns) == 1:
        axes = [axes]

    labels = [
        ("frontend_bound", "Frontend_Bound"),
        ("bad_speculation", "Bad_Speculation"),
        ("backend_bound", "Backend_Bound"),
        ("retiring", "Retiring"),
    ]

    for ax, rep_n in zip(axes, rep_ns):
        part = df[df["n"] == rep_n].copy()
        part["mode"] = pd.Categorical(part["mode"], categories=mode_order, ordered=True)
        part = part.sort_values("mode")

        vals = part[required_cols].copy().fillna(0.0)
        row_sums = vals.sum(axis=1).replace(0, np.nan)
        norm = vals.div(row_sums, axis=0) * 100.0
        norm = norm.fillna(0.0)

        x = np.arange(len(part))
        bottom = np.zeros(len(part))

        for col, label in labels:
            y = norm[col].to_numpy(dtype=float)
            ax.bar(x, y, bottom=bottom, label=label)
            bottom += y

        ax.set_xticks(x)
        ax.set_xticklabels(part["mode"].astype(str).tolist(), rotation=20)
        ax.set_ylim(0, 100)
        ax.set_ylabel("Normalized contribution (%)")
        ax.set_title(f"n = {rep_n}")
        ax.grid(True, axis="y", alpha=0.3)

    handles, labels = axes[0].get_legend_handles_labels()
    if handles:
        fig.legend(handles, labels, loc="upper center", ncol=2, bbox_to_anchor=(0.5, 1.02))
    fig.suptitle("Sum top-down 100% stacked bars", y=1.10, fontsize=14)

    save_figure(fig, out_dir / "sum_topdown_stacked.png")


def main() -> None:
    args = parse_args()

    matrix_summary = args.matrix_summary or (args.base_dir / "matrix_summary.csv")
    sum_summary = args.sum_summary or (args.base_dir / "sum_summary.csv")
    out_dir = args.out_dir

    ensure_dir(out_dir)

    matrix_df = read_matrix_summary(matrix_summary)
    sum_df = read_sum_summary(sum_summary)

    plot_matrix_profiler_compare(matrix_df, out_dir)
    plot_matrix_performance_cause(matrix_df, out_dir)
    plot_sum_profiler_compare(sum_df, out_dir)
    plot_sum_topdown_stacked(sum_df, out_dir)

    print(f"Saved advanced figures to: {out_dir.resolve()}")


if __name__ == "__main__":
    main()
