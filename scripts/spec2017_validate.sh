#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
suite=${SPEC2017_ROOT:-"$repo/work/spec2017/install"}
cp "$repo/configs/spec_cpu2017_gcc11_O2.cfg" "$suite/config/spec_cpu2017_gcc11_O2.cfg"
cd "$suite"
set +u
source shrc
set -u
export OMP_NUM_THREADS=1
export TZ=UTC
for app in 503.bwaves_r 505.mcf_r 507.cactuBSSN_r 519.lbm_r 520.omnetpp_r 523.xalancbmk_r 549.fotonik3d_r 557.xz_r; do
  runcpu --config=spec_cpu2017_gcc11_O2 --define build_ncpus=8 --tune=base --copies=1 --iterations=1 --action=validate --size=ref "$app"
done
