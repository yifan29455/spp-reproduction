import csv
import math
from collections import defaultdict


def read_csv(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=sorted({key for row in rows for key in row}), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def ratio(numerator, denominator):
    return numerator / denominator if denominator else ""


def add_baselines(rows, declared):
    indexed = {row["run_id"]: row for row in rows}
    for row in rows:
        baseline = indexed[declared[row["run_id"]]]
        if baseline["prefetcher"] != "no_prefetch":
            raise ValueError("Expected a no-prefetch baseline")
        for field in ("case", "bandwidth", "warmup", "roi"):
            if row[field] != baseline[field]:
                raise ValueError(f"Baseline condition differs: {field}")
        row["baseline_ipc"] = baseline["actual_roi_instructions"] / baseline["roi_cycles"]
        row["roi_ipc"] = row["actual_roi_instructions"] / row["roi_cycles"]
        row["speedup"] = row["roi_ipc"] / row["baseline_ipc"]


def application_tables(rows):
    grouped = defaultdict(list)
    for row in rows:
        if row["group"] in ("A1", "B1"):
            application = row["case"].removesuffix("_w1").removesuffix("_w2")
            grouped[(row["group"], application, row["prefetcher"])].append(row)
    applications = []
    for (group, application, variant), values in sorted(grouped.items()):
        baseline = grouped[(group, application, "no_prefetch")]
        total = lambda items, key: sum(item[key] for item in items)
        instructions, cycles = total(values, "actual_roi_instructions"), total(values, "roi_cycles")
        baseline_ipc = total(baseline, "actual_roi_instructions") / total(baseline, "roi_cycles")
        created, timely, late, unused, unresolved = [total(values, f"l2_pf_roi_cohort_{suffix}")
            for suffix in ("created", "timely", "late", "unused", "unresolved_end")]
        misses = total(values, "l2_load_miss") + total(values, "l2_rfo_miss")
        hits = total(values, "l2_load_hit") + total(values, "l2_rfo_hit")
        merges = total(values, "l2_load_miss_merge") + total(values, "l2_rfo_miss_merge")
        baseline_misses = total(baseline, "l2_load_miss") + total(baseline, "l2_rfo_miss")
        reads, writes = total(values, "dram_read_transactions_proxy"), total(values, "dram_write_transactions_proxy")
        baseline_dram = total(baseline, "dram_read_transactions_proxy") + total(baseline, "dram_write_transactions_proxy")
        applications.append({"group": group, "application": application, "prefetcher": variant, "windows": len(values),
            "roi_instructions": instructions, "roi_cycles": cycles, "ipc": instructions / cycles,
            "speedup": (instructions / cycles) / baseline_ipc, "l2_mpki": 1000 * misses / instructions,
            "l2_mpki_including_merge": 1000 * (misses + merges) / instructions, "l2_miss_rate": ratio(misses, hits + misses),
            "l2_accuracy": ratio(timely + late, created), "l2_timely_accuracy": ratio(timely, created),
            "l2_coverage": ratio(timely, baseline_misses), "l2_created": created, "l2_timely": timely, "l2_late": late,
            "l2_unused": unused, "l2_unresolved": unresolved, "dram_reads": reads, "dram_writes": writes,
            "dram_transactions_per_ki": 1000 * (reads + writes) / instructions, "dram_ratio": ratio(reads + writes, baseline_dram)})
    geometric = []
    for group in ("A1", "B1"):
        for variant in ("no_prefetch", "spp", "bop"):
            values = [row["speedup"] for row in applications if row["group"] == group and row["prefetcher"] == variant]
            geometric.append({"group": group, "prefetcher": variant, "applications": len(values),
                              "geometric_mean_speedup": math.exp(sum(math.log(value) for value in values) / len(values))})
    return applications, geometric
