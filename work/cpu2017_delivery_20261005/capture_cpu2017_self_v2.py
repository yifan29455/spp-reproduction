import argparse
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APPS = {"bwaves": "503.bwaves_r", "mcf": "505.mcf_r", "cactubssn": "507.cactuBSSN_r",
        "lbm": "519.lbm_r", "omnetpp": "520.omnetpp_r", "xalancbmk": "523.xalancbmk_r",
        "fotonik3d": "549.fotonik3d_r", "xz": "557.xz_r"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--case", choices=APPS, required=True)
    parser.add_argument("--window", choices=("w1", "w2"), required=True)
    parser.add_argument("--run-dir", type=Path)
    parser.add_argument("--skip", type=int)
    parser.add_argument("--records", type=int, default=1_210_000_000)
    parser.add_argument("--call-index", type=int, default=0)
    parser.add_argument("--trace-root", type=Path, required=True)
    args = parser.parse_args()
    skip = args.skip if args.skip is not None else (1_000_000_000 if args.window == "w1" else 10_000_000_000)
    if skip < 0 or args.records <= 0:
        parser.error("Invalid instruction count")
    spec = Path(os.environ["SPEC2017_ROOT"])
    pin = Path(os.environ["PIN_ROOT"]) / "pin"
    tracer = Path(os.environ.get("TRACER_SO", ROOT / "work/tracer_v2_build/tracer/pin/obj-intel64/champsim_tracer.so"))
    if args.run_dir:
        run_dir = args.run_dir.resolve(strict=True)
    else:
        candidates = list((spec / "benchspec/CPU" / APPS[args.case] / "run").glob("run_base_refrate_*"))
        if len(candidates) != 1:
            raise ValueError("Specify --run-dir when more than one SPEC run directory exists")
        run_dir = candidates[0].resolve()
    invocation = subprocess.run([str(spec / "bin/specinvoke"), "-n"], cwd=run_dir, text=True,
                                stdout=subprocess.PIPE, check=True).stdout
    commands = [line.strip() for line in invocation.splitlines() if line.strip()
                and not line.lstrip().startswith("#") and not line.startswith("specinvoke exit:")]
    command = commands[args.call_index]
    args.trace_root.mkdir(parents=True, exist_ok=True)
    trace = args.trace_root.resolve() / f"{args.case}_{args.window}.champsimtrace.xz"
    env = dict(os.environ, OMP_NUM_THREADS="1", TZ="UTC")
    for line in (run_dir / "speccmds.cmd").read_text().splitlines():
        if line.startswith("-E "):
            _, key, value = shlex.split(line)
            env[key] = value
    env["OMP_NUM_THREADS"] = "1"
    env["TZ"] = "UTC"
    # FIFO 将 trace 直接送入 xz。
    with tempfile.TemporaryDirectory(prefix="spp-capture-") as temporary, trace.open("xb") as output:
        fifo = Path(temporary) / "trace.fifo"
        os.mkfifo(fifo)
        compressor = subprocess.Popen(["xz", "-T1", "-2", "-c", str(fifo)], stdout=output)
        prefix = [str(pin), "-t", str(tracer), "-s", str(skip), "-t", str(args.records), "-o", str(fifo), "--"]
        process = subprocess.Popen(["bash", "-c", shlex.join(prefix) + " " + command], cwd=run_dir, env=env)
        returncode = process.wait()
        if returncode:
            compressor.terminate()
        compression_code = compressor.wait()
        if returncode or compression_code:
            raise RuntimeError(f"Capture failed: Pin={returncode}, xz={compression_code}")
    print(f"Captured {trace.name}")


if __name__ == "__main__":
    main()
