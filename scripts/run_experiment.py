import argparse
import subprocess
from pathlib import Path

from build_submission import ROOT, VARIANTS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--variant", choices=VARIANTS, required=True)
    parser.add_argument("--build-prefix", default="")
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--results-scope", default="manual")
    parser.add_argument("--warmup", type=int, default=200_000_000)
    parser.add_argument("--simulation", type=int, default=1_000_000_000)
    args = parser.parse_args()
    if args.warmup < 0 or args.simulation <= 0:
        parser.error("Invalid instruction count")
    for name in (args.run_id, args.results_scope, args.build_prefix or args.variant):
        if Path(name).name != name or name in (".", ".."):
            parser.error("Use a directory name without path separators")
    trace = args.trace.resolve(strict=True)
    directory = ROOT / "results/raw" / args.results_scope / args.run_id
    directory.mkdir(parents=True, exist_ok=False)
    binary = ROOT / "third_party/ChampSim/build" / (args.build_prefix or args.variant) / "bin/spp-paper"
    command = [str(binary), "--hide-heartbeat", "--warmup-instructions", str(args.warmup),
               "--simulation-instructions", str(args.simulation), "--json", str(directory / "result.json"), str(trace)]
    with (directory / "stdout.txt").open("w") as output:
        subprocess.run(command, stdout=output, check=True)
    print(f"Completed {args.run_id}")


if __name__ == "__main__":
    main()
