import json


def total(value):
    return sum(value) if isinstance(value, list) else value


def load_phases(path):
    with path.open() as stream:
        return json.load(stream)


def phase_metrics(phases):
    metrics = {"roi_instructions": 0, "roi_cycles": 0}
    fields = {"ROI prefetch cohort created": "created", "ROI prefetch cohort timely useful": "timely",
              "ROI prefetch cohort late useful": "late", "ROI prefetch cohort unused": "unused",
              "ROI prefetch cohort unresolved at end": "unresolved_end"}
    for phase in phases:
        roi = phase["roi"]
        for core in roi["cores"]:
            metrics["roi_instructions"] += core["instructions"]
            metrics["roi_cycles"] += core["cycles"]
        for level, name in (("l1d", "cpu0_L1D"), ("l2", "cpu0_L2C"), ("llc", "LLC")):
            cache = roi[name]
            for request in ("LOAD", "RFO"):
                for field in ("hit", "miss", "miss_merge"):
                    key = f"{level}_{request.lower()}_{field}"
                    metrics[key] = metrics.get(key, 0) + total(cache[request][field])
            for field, suffix in fields.items():
                key = f"{level}_pf_roi_cohort_{suffix}"
                metrics[key] = metrics.get(key, 0) + total(cache[field])
        for channel in roi["DRAM"]:
            for prefix, requests in (("read", "RQ"), ("write", "WQ")):
                key = f"dram_{prefix}_transactions_proxy"
                transactions = channel[f"{requests} ROW_BUFFER_HIT"] + channel[f"{requests} ROW_BUFFER_MISS"]
                metrics[key] = metrics.get(key, 0) + transactions
    return metrics
