# Experiment scripts and execution logs

The 114 simulations are listed in `manifests/report_runs.csv`.
`experiments/execution_index.csv` links each simulation to its result, output log and run record.
The recorded start and finish times use UTC.

## Experiment groups

| Group | Experiment | Runs | Settings |
|---|---|---:|---|
| A1 | CPU2006 main comparison | 24 | Eight applications, no prefetching, SPP and BOP |
| A2 | CPU2006 component comparison | 4 | GemsFDTD and libquantum, Basic and Lookahead |
| A3 | CPU2006 threshold and bandwidth | 12 | bwaves and mcf, additional threshold settings and low-bandwidth baselines |
| A4 | CPU2006 ROI length | 6 | bwaves and mcf, 500M ROI and three prefetcher settings |
| B1 | CPU2017 main comparison | 48 | Eight applications, two windows and three prefetcher settings |
| B3 | CPU2017 component comparison | 8 | bwaves and fotonik3d, two windows, Basic and Lookahead |
| B4 | CPU2017 threshold and bandwidth | 12 | bwaves and mcf, window w1, additional threshold settings and low-bandwidth baselines |

Full SPP and no-prefetch results are reused from A1 and B1 where the trace and settings match. The 1B ROI comparison reuses A1. Normal-bandwidth results at the 25% threshold also reuse the main comparison.

## Scripts

| Script | Purpose |
|---|---|
| `scripts/build_submission.py` | Build a simulator for a prefetcher variant and bandwidth setting |
| `scripts/run_selected.py` | Select a group or run from the manifest, build the required variants and execute simulations |
| `scripts/run_experiment.py` | Execute one simulation and save JSON results and standard output |
| `scripts/parse_results.py` | Read instruction, cycle, cache and memory statistics from native JSON |
| `scripts/reproduce_report.py` | Generate result tables and speedup plots from the supplied results |
| `scripts/spec2017_validate.sh` | Build and check the selected CPU2017 programs |
| `scripts/build_tracer331.sh` | Build the Pin tracer |
| `work/cpu2017_delivery_20261005/capture_cpu2017_self_v2.py` | Capture a CPU2017 execution window |
| `work/cpu2017_delivery_20261005/compose_delivery_results.py` | Match baselines and aggregate application results |

## Run a group

Run these commands from the repository root on Linux x86-64. Set `INPUT_ROOT` to the directory containing the relative trace paths in `manifests/external_traces.csv`.

```bash
python3 scripts/run_selected.py --group A1 --input-root "$INPUT_ROOT" --scope cpu2006_check
```

Use `--group A2`, `A3`, `A4`, `B1`, `B3` or `B4` to select another group. Use a new `--scope` value for each execution. The manifest supplies the prefetcher variant, bandwidth, threshold, warmup and ROI length for every run.

## Run one simulation

```bash
python3 scripts/build_submission.py spp
python3 scripts/run_experiment.py \
  --run-id bwaves_spp_check \
  --variant spp \
  --trace "$TRACE_FILE" \
  --results-scope manual_check \
  --warmup 200000000 \
  --simulation 1000000000
```

`TRACE_FILE` identifies the prepared trace. The new output is saved in `results/raw/manual_check/bwaves_spp_check/`.

Build variants are `no`, `bop`, `spp`, `spp_basic`, `spp_lookahead`, `spp_t15` and `spp_t40`. For low bandwidth, build with `--bandwidth low` and pass the resulting build prefix, such as `spp_t40_low`, to `run_experiment.py --build-prefix`.

## Capture CPU2017 input

Set `SPEC2017_ROOT` and `PIN_ROOT` to the local SPEC CPU2017 v1.1.9 and Pin 3.31 installations.

```bash
bash scripts/spec2017_validate.sh
bash scripts/build_tracer331.sh
python3 work/cpu2017_delivery_20261005/capture_cpu2017_self_v2.py \
  --case bwaves --window w1 --trace-root "$PWD/work/new_traces"
```

Window w1 skips 1B dynamic instructions and w2 skips 10B. Each trace contains 1.21B records. Capture the two windows of the same application sequentially.

## Rebuild the tables

```bash
python3 -m pip install -r requirements.txt
python3 scripts/reproduce_report.py
```

The script reads the supplied results and writes the six CSV tables and two speedup plots under `work/cpu2017_delivery_20261005/delivery_results/`.

## Read a recorded run

Find the run in `execution_index.csv`. Its `log_file` points to the simulator output, and its `record_file` gives the recorded command, settings and times. The `result_file` points to the native JSON used for analysis. The `baseline_run_id` identifies the corresponding no-prefetch run.

The main comparisons use 200M warmup and 1B ROI. Group A4 uses the same warmup and a 500M ROI. CPU2017 application IPC combines instruction and cycle counts from w1 and w2; the CPU2017 threshold comparison uses w1.

