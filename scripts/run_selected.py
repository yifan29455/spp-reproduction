import argparse
import csv
import subprocess
import sys
from pathlib import Path

from build_submission import ROOT


def main():
    parser = argparse.ArgumentParser()
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--run-id")
    selection.add_argument("--group", choices=("A1", "A2", "A3", "A4", "B1", "B3", "B4", "all"))
    parser.add_argument("--input-root", type=Path, required=True)
    parser.add_argument("--scope", required=True)
    parser.add_argument("--dependency-root", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.scope in {"final_cpu2017", "cpu2017_delivery_20261005"} or Path(args.scope).name != args.scope:
        raise ValueError("Use a new result directory name")
    rows = list(csv.DictReader((ROOT / "manifests/report_runs.csv").open()))
    traces = {(row["suite"].lower(), row["case"]): row["relative_path"]
              for row in csv.DictReader((ROOT / "manifests/external_traces.csv").open())}
    rows = [r for r in rows if r["run_id"] == args.run_id or args.group in (r["group"], "all")]
    if not rows:
        raise ValueError("No matching experiment")
    built = set()
    for row in rows:
        variant = "no" if row["prefetcher"] == "no_prefetch" else row["prefetcher"]
        if variant == "spp" and row["prefetch_threshold"] in ("0.15", "0.4", "0.40"):
            variant = "spp_t15" if float(row["prefetch_threshold"]) == 0.15 else "spp_t40"
        prefix = variant + ("_low" if row["bandwidth"] == "low" else "")
        if prefix not in built:
            command = [sys.executable, str(ROOT / "scripts/build_submission.py"), variant,
                       "--bandwidth", row["bandwidth"], "--jobs", str(args.jobs)]
            if args.dependency_root:
                command.extend(["--dependency-root", str(args.dependency_root)])
            subprocess.run(command, check=True)
            built.add(prefix)
        trace = args.input_root.resolve() / traces[(row["suite"].lower(), row["case"])]
        subprocess.run([sys.executable, str(ROOT / "scripts/run_experiment.py"), "--run-id", row["run_id"],
                        "--variant", variant, "--build-prefix", prefix, "--trace", str(trace),
                        "--results-scope", args.scope, "--warmup", row["warmup"], "--simulation", row["roi"]], check=True)


if __name__ == "__main__":
    main()
