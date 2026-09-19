#ifndef CACHE_STATS_H
#define CACHE_STATS_H

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "channel.h"
#include "event_counter.h"

struct cache_stats {
  std::string name;
  // prefetch stats
  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_late_useful = 0;
  uint64_t pf_upstream_received = 0;
  uint64_t pf_fill = 0;

  // Ordered hash of CACHE::prefetch_line API candidate requests in this phase.
  // This common observer is retained in both lifecycle-counter on/off builds.
  uint64_t pf_request_seq_count = 0;
  uint64_t pf_request_seq_hash = 14695981039346656037ULL;
  uint64_t pf_warmup_request_seq_count = 0;
  uint64_t pf_warmup_request_seq_hash = 14695981039346656037ULL;

  // Prefetch API/request attempts that did not create a new local MSHR transaction.
  uint64_t pf_attempt_pq_rejected = 0;
  uint64_t pf_attempt_hit_existing = 0;
  uint64_t pf_attempt_merged_prefetch = 0;
  uint64_t pf_attempt_merged_demand = 0;
  uint64_t pf_attempt_mshr_rejected = 0;
  uint64_t pf_attempt_lower_queue_rejected = 0;
  uint64_t pf_attempt_no_local_fill = 0;

  // Per-cache transaction cohort created by ROI-originated prefetches.
  // Conservation: created = timely + late + unused + unresolved.
  uint64_t pf_roi_cohort_created = 0;
  uint64_t pf_roi_cohort_timely = 0;
  uint64_t pf_roi_cohort_late = 0;
  uint64_t pf_roi_cohort_unused = 0;
  uint64_t pf_roi_cohort_unused_evicted = 0;
  uint64_t pf_roi_cohort_unused_invalidated = 0;
  uint64_t pf_roi_cohort_unused_other = 0;
  uint64_t pf_roi_cohort_unresolved_end = 0;
  uint64_t pf_roi_cohort_balance_error = 0;

  // Warmup-origin activity crossing into ROI is reported separately, not
  // mixed into the ROI-created cohort denominator.
  uint64_t pf_warmup_active_transactions_roi_start = 0;
  uint64_t pf_warmup_pending_requests_roi_start = 0;
  uint64_t pf_warmup_transactions_created_roi = 0;
  uint64_t pf_warmup_timely_roi = 0;
  uint64_t pf_warmup_late_roi = 0;
  uint64_t pf_warmup_unused_roi = 0;
  uint64_t pf_warmup_unused_evicted_roi = 0;
  uint64_t pf_warmup_unused_invalidated_roi = 0;
  uint64_t pf_warmup_unused_other_roi = 0;
  uint64_t pf_warmup_pending_requests_roi_end = 0;
  uint64_t pf_warmup_unresolved_roi_end = 0;
  uint64_t pf_warmup_cohort_balance_error_roi = 0;
  uint64_t pf_roi_pending_requests_roi_end = 0;

  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> hits = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> misses = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> miss_merge = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> fill = {};

  long total_miss_latency_cycles{};
};

cache_stats operator-(cache_stats lhs, cache_stats rhs);

#endif
