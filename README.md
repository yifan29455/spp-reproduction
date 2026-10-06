# Signature Path Prefetcher experiments

This project compares SPP, BOP and no prefetching on SPEC CPU2006 and CPU2017. It also examines Lookahead and GHR, ROI length on CPU2006, and confidence thresholds at two memory bandwidths.

## Files

- `report/SPP_Technical_Report.tex`: technical report source. Appendix B describes AI use.
- `patches/`: SPP, BOP and the ChampSim changes.
- `configs/`: processor, prefetcher and SPEC compilation settings.
- `scripts/`: build, simulation and analysis scripts.
- `manifests/report_runs.csv`: the 114 experiments used in the report.
- `results/raw/`: native simulation results.
- `work/cpu2017_delivery_20261005/`: CPU2017 capture script, result tables and figures.

| Group | Comparison | Runs |
|---|---|---:|
| A1 | CPU2006 no-prefetch, SPP and BOP | 24 |
| A2 | CPU2006 Basic SPP and Lookahead | 4 |
| A3 | CPU2006 threshold and bandwidth | 12 |
| A4 | CPU2006 500M ROI | 6 |
| B1 | CPU2017 no-prefetch, SPP and BOP | 48 |
| B3 | CPU2017 Basic SPP and Lookahead | 8 |
| B4 | CPU2017 threshold and bandwidth | 12 |

Full SPP and baseline results are reused where the settings are the same. CPU2017 uses two windows per application. Application IPC is calculated as the total instructions divided by the total cycles.

## Analyze the results

Use Python 3.10 or later:

```bash
python3 -m pip install -r requirements.txt
python3 scripts/reproduce_report.py
```

The script reads the supplied results and generates the tables and speedup charts.

## Build and run

Use Linux x86-64, GCC/G++ 11 and the ChampSim dependencies. The included source contains the algorithm changes.

```bash
python3 scripts/build_submission.py no
python3 scripts/build_submission.py spp
python3 scripts/build_submission.py bop
python3 scripts/run_selected.py --group A1 --input-root "$INPUT_ROOT" --scope cpu2006_run
```

`INPUT_ROOT` contains the relative trace paths listed in `manifests/external_traces.csv`. Public CPU2006 trace links are included there. CPU2017 traces are generated from the course-provided SPEC CPU2017 v1.1.9 installation.

Additional build variants are `spp_basic`, `spp_lookahead`, `spp_t15` and `spp_t40`. Use `--bandwidth low` for the lower-bandwidth configuration. `--dependency-root /path/to/ChampSim` can reuse installed build dependencies.

## Capture CPU2017 traces

Set `SPEC2017_ROOT` to the installed SPEC suite and `PIN_ROOT` to Pin 3.31. Use the supplied GCC 11 configuration:

```bash
bash scripts/spec2017_validate.sh
bash scripts/build_tracer331.sh
python3 work/cpu2017_delivery_20261005/capture_cpu2017_self_v2.py --case bwaves --window w1 --trace-root "$PWD/work/new_traces"
```

The applications are bwaves, mcf, cactubssn, lbm, omnetpp, xalancbmk, fotonik3d and xz. Each capture records 1.21 billion instructions, skipping 1 billion for `w1` or 10 billion for `w2`. Capture each application's windows sequentially.

The English report can be compiled with XeLaTeX. ChampSim's license is included with its source.
