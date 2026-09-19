import argparse
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHAMPSIM = ROOT / "third_party/ChampSim"
VARIANTS = {
    "no": ("no_prefetch.json", []),
    "bop": ("bop.json", []),
    "spp": ("spp.json", [1, 1, 25]),
    "spp_basic": ("spp_basic.json", [0, 0, 25]),
    "spp_lookahead": ("spp_lookahead.json", [1, 0, 25]),
    "spp_t15": ("spp.json", [1, 1, 15]),
    "spp_t40": ("spp.json", [1, 1, 40]),
}


def dependencies(root):
    if root:
        root = root.resolve()
        for name in ("vcpkg", "vcpkg_installed"):
            source, target = root / name, CHAMPSIM / name
            if not source.is_dir():
                raise FileNotFoundError(source)
            if target.exists():
                if target.resolve() != source:
                    raise ValueError(f"Dependency directory already exists: {target}")
            else:
                target.symlink_to(source, target_is_directory=True)
        if not (CHAMPSIM / "vcpkg_installed/x64-linux/include/fmt").is_dir():
            raise ValueError("The dependency directory must contain the installed x64-linux libraries")
        return
    kit = CHAMPSIM / "vcpkg"
    if not kit.exists():
        commit = (ROOT / "third_party/ChampSim.VCPKG_COMMIT").read_text().strip()
        subprocess.run(["git", "clone", "https://github.com/microsoft/vcpkg.git", str(kit)], check=True)
        subprocess.run(["git", "-C", str(kit), "checkout", "--detach", commit], check=True)
    env = dict(os.environ, VCPKG_FORCE_SYSTEM_BINARIES="1")
    if not (kit / "vcpkg").is_file():
        subprocess.run(["bash", str(kit / "bootstrap-vcpkg.sh"), "-disableMetrics"], env=env, check=True)
    subprocess.run([str(kit / "vcpkg"), "install", "--disable-metrics", "--x-manifest-root=.",
                    "--x-install-root=./vcpkg_installed", "--triplet=x64-linux"], cwd=CHAMPSIM, env=env, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("variant", choices=VARIANTS)
    parser.add_argument("--bandwidth", choices=("normal", "low"), default="normal")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--dependency-root", type=Path)
    args = parser.parse_args()
    dependencies(args.dependency_root)
    prefix = args.variant + ("_low" if args.bandwidth == "low" else "")
    build = CHAMPSIM / "build" / prefix
    build.mkdir(parents=True, exist_ok=True)
    base = ROOT / "configs/paper_base.json"
    if args.bandwidth == "low":
        base = ROOT / "configs/paper_base_low_bw.json"
    overlay, values = VARIANTS[args.variant]
    flags = []
    if values:
        flags = [f"-DSPP_LOOKAHEAD_ENABLED={values[0]}", f"-DSPP_GHR_ENABLED={values[1]}",
                 f"-DSPP_PREFETCH_THRESHOLD={values[2]}"]
        if args.variant != "spp_basic":
            flags.append("-DSPP_LOOKAHEAD_MAX_DEPTH=256")
    subprocess.run([str(CHAMPSIM / "config.sh"), "--no-compile-all-modules", "--prefix", str(build),
                    "--bindir", str(build / "bin"), "--makedir", str(build), str(base), str(ROOT / "configs" / overlay)], check=True)
    subprocess.run(["make", "-I" + str(build), "OBJ_ROOT=" + str(build / ".csconfig"),
                    "BIN_ROOT=" + str(build / "bin"), "CPPFLAGS=" + " ".join(flags), f"-j{args.jobs}"], cwd=CHAMPSIM, check=True)
    binaries = [p for p in (build / "bin").iterdir() if p.is_file() and os.access(p, os.X_OK)]
    if len(binaries) != 1:
        raise ValueError("Expected one compiled simulator")
    print(f"Built {binaries[0].relative_to(ROOT)}")


if __name__ == "__main__":
    main()
