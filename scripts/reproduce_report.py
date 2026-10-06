import argparse
import shutil
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "work/cpu2017_delivery_20261005"
sys.path.insert(0, str(STAGE))
import compose_delivery_results as native
import parse_results


def aggregate(values, indexed):
    baselines = [indexed[row["baseline_run_id"]] for row in values]
    instructions = sum(r["actual_roi_instructions"] for r in values)
    cycles = sum(r["roi_cycles"] for r in values)
    baseline_ipc = sum(r["actual_roi_instructions"] for r in baselines) / sum(r["roi_cycles"] for r in baselines)
    return {"windows": len(values), "instructions": instructions, "cycles": cycles,
            "ipc": instructions / cycles, "speedup": instructions / cycles / baseline_ipc,
            "run_ids": "|".join(r["run_id"] for r in values),
            "baseline_ids": "|".join(r["run_id"] for r in baselines)}


def extra_tables(rows):
    indexed = {r["run_id"]: r for r in rows}
    components, thresholds, windows = [], [], []
    for suite, main, component, cases in (("CPU2006", "A1", "A2", ("GemsFDTD", "libquantum")),
                                        ("CPU2017", "B1", "B3", ("bwaves", "fotonik3d"))):
        for case in cases:
            for variant in ("spp_basic", "spp_lookahead", "spp"):
                group = main if variant == "spp" else component
                values = [r for r in rows if r["group"] == group and r["prefetcher"] == variant
                          and r["case"].removesuffix("_w1").removesuffix("_w2") == case]
                components.append({"suite": suite, "application": case, "prefetcher": variant, **aggregate(values, indexed)})
    for suite, main, group in (("CPU2006", "A1", "A3"), ("CPU2017", "B1", "B4")):
        for case in ("bwaves", "mcf"):
            selected_case = case if suite == "CPU2006" else case + "_w1"
            for bandwidth in ("normal", "low"):
                for threshold in (0.15, 0.25, 0.40):
                    selected_group = main if bandwidth == "normal" and threshold == 0.25 else group
                    values = [r for r in rows if r["group"] == selected_group and r["case"] == selected_case
                              and r["prefetcher"] == "spp" and r["bandwidth"] == bandwidth
                              and float(r["prefetch_threshold"]) == threshold]
                    thresholds.append({"suite": suite, "application": case, "bandwidth": bandwidth,
                                       "threshold": threshold, **aggregate(values, indexed)})
    for case in ("bwaves", "mcf"):
        for group in ("A4", "A1"):
            for variant in ("no_prefetch", "spp", "bop"):
                values = [r for r in rows if r["group"] == group and r["case"] == case and r["prefetcher"] == variant]
                windows.append({"application": case, "roi": values[0]["roi"], "prefetcher": variant, **aggregate(values, indexed)})
    return components, thresholds, windows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=STAGE / "delivery_results")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = native.read_csv(ROOT / "manifests/report_runs.csv")
    for row in rows:
        row.update(parse_results.phase_metrics(parse_results.load_phases(ROOT / row["result_file"])))
        row["actual_roi_instructions"] = row["roi_instructions"]
    native.add_baselines(rows, {r["run_id"]: r["baseline_run_id"] for r in rows})
    applications, means = native.application_tables(rows)
    components, thresholds, windows = extra_tables(rows)
    tables = {"formal_all.csv": rows, "application_metrics.csv": applications,
              "application_geomeans.csv": means, "ablation_metrics.csv": components,
              "threshold_metrics.csv": thresholds, "roi_metrics.csv": windows}
    for name, values in tables.items():
        native.write_csv(args.output / name, values)
    for group, suite in (("A1", "CPU2006"), ("B1", "CPU2017")):
        names = sorted({r["application"] for r in applications if r["group"] == group})
        fig, ax = plt.subplots(figsize=(9, 4))
        for offset, variant, color in ((-0.18, "spp", "#0072B2"), (0.18, "bop", "#D55E00")):
            values = [next(r["speedup"] for r in applications if r["group"] == group and r["application"] == name
                           and r["prefetcher"] == variant) for name in names]
            ax.bar([i + offset for i in range(len(names))], values, width=0.36, color=color, label=variant.upper())
        ax.set_xticks(range(len(names)), names, rotation=25, ha="right")
        ax.set_ylabel("Speedup")
        ax.axhline(1, color="gray", linestyle="--", linewidth=0.8)
        ax.legend()
        fig.tight_layout()
        fig.savefig(args.output / f"{suite.lower()}_speedup.png", dpi=180)
        plt.close(fig)
    if args.output == STAGE / "delivery_results":
        metrics = STAGE / "report_metrics"
        metrics.mkdir(exist_ok=True)
        for name in ("ablation_metrics.csv", "threshold_metrics.csv"):
            shutil.copy2(args.output / name, metrics / name)
    print(f"Processed {len(rows)} experiments")


if __name__ == "__main__":
    main()
