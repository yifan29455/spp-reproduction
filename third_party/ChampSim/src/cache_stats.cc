#include "cache_stats.h"

cache_stats operator-(cache_stats lhs, cache_stats rhs)
{
  cache_stats result;
  result.pf_requested = lhs.pf_requested - rhs.pf_requested;
  result.pf_issued = lhs.pf_issued - rhs.pf_issued;
  result.pf_useful = lhs.pf_useful - rhs.pf_useful;
  result.pf_useless = lhs.pf_useless - rhs.pf_useless;
  result.pf_late_useful = lhs.pf_late_useful - rhs.pf_late_useful;
  result.pf_upstream_received = lhs.pf_upstream_received - rhs.pf_upstream_received;
  result.pf_fill = lhs.pf_fill - rhs.pf_fill;
  result.pf_request_seq_count = lhs.pf_request_seq_count - rhs.pf_request_seq_count;
  result.pf_request_seq_hash = lhs.pf_request_seq_hash;
  result.pf_warmup_request_seq_count = lhs.pf_warmup_request_seq_count;
  result.pf_warmup_request_seq_hash = lhs.pf_warmup_request_seq_hash;
  result.pf_attempt_pq_rejected = lhs.pf_attempt_pq_rejected - rhs.pf_attempt_pq_rejected;
  result.pf_attempt_hit_existing = lhs.pf_attempt_hit_existing - rhs.pf_attempt_hit_existing;
  result.pf_attempt_merged_prefetch = lhs.pf_attempt_merged_prefetch - rhs.pf_attempt_merged_prefetch;
  result.pf_attempt_merged_demand = lhs.pf_attempt_merged_demand - rhs.pf_attempt_merged_demand;
  result.pf_attempt_mshr_rejected = lhs.pf_attempt_mshr_rejected - rhs.pf_attempt_mshr_rejected;
  result.pf_attempt_lower_queue_rejected = lhs.pf_attempt_lower_queue_rejected - rhs.pf_attempt_lower_queue_rejected;
  result.pf_attempt_no_local_fill = lhs.pf_attempt_no_local_fill - rhs.pf_attempt_no_local_fill;
  result.pf_roi_cohort_created = lhs.pf_roi_cohort_created - rhs.pf_roi_cohort_created;
  result.pf_roi_cohort_timely = lhs.pf_roi_cohort_timely - rhs.pf_roi_cohort_timely;
  result.pf_roi_cohort_late = lhs.pf_roi_cohort_late - rhs.pf_roi_cohort_late;
  result.pf_roi_cohort_unused = lhs.pf_roi_cohort_unused - rhs.pf_roi_cohort_unused;
  result.pf_roi_cohort_unused_evicted = lhs.pf_roi_cohort_unused_evicted - rhs.pf_roi_cohort_unused_evicted;
  result.pf_roi_cohort_unused_invalidated = lhs.pf_roi_cohort_unused_invalidated - rhs.pf_roi_cohort_unused_invalidated;
  result.pf_roi_cohort_unused_other = lhs.pf_roi_cohort_unused_other - rhs.pf_roi_cohort_unused_other;
  result.pf_roi_cohort_unresolved_end = lhs.pf_roi_cohort_unresolved_end - rhs.pf_roi_cohort_unresolved_end;
  result.pf_roi_cohort_balance_error = lhs.pf_roi_cohort_balance_error - rhs.pf_roi_cohort_balance_error;
  result.pf_warmup_active_transactions_roi_start = lhs.pf_warmup_active_transactions_roi_start - rhs.pf_warmup_active_transactions_roi_start;
  result.pf_warmup_pending_requests_roi_start = lhs.pf_warmup_pending_requests_roi_start - rhs.pf_warmup_pending_requests_roi_start;
  result.pf_warmup_transactions_created_roi = lhs.pf_warmup_transactions_created_roi - rhs.pf_warmup_transactions_created_roi;
  result.pf_warmup_timely_roi = lhs.pf_warmup_timely_roi - rhs.pf_warmup_timely_roi;
  result.pf_warmup_late_roi = lhs.pf_warmup_late_roi - rhs.pf_warmup_late_roi;
  result.pf_warmup_unused_roi = lhs.pf_warmup_unused_roi - rhs.pf_warmup_unused_roi;
  result.pf_warmup_unused_evicted_roi = lhs.pf_warmup_unused_evicted_roi - rhs.pf_warmup_unused_evicted_roi;
  result.pf_warmup_unused_invalidated_roi = lhs.pf_warmup_unused_invalidated_roi - rhs.pf_warmup_unused_invalidated_roi;
  result.pf_warmup_unused_other_roi = lhs.pf_warmup_unused_other_roi - rhs.pf_warmup_unused_other_roi;
  result.pf_warmup_pending_requests_roi_end = lhs.pf_warmup_pending_requests_roi_end - rhs.pf_warmup_pending_requests_roi_end;
  result.pf_warmup_unresolved_roi_end = lhs.pf_warmup_unresolved_roi_end - rhs.pf_warmup_unresolved_roi_end;
  result.pf_warmup_cohort_balance_error_roi = lhs.pf_warmup_cohort_balance_error_roi - rhs.pf_warmup_cohort_balance_error_roi;
  result.pf_roi_pending_requests_roi_end = lhs.pf_roi_pending_requests_roi_end - rhs.pf_roi_pending_requests_roi_end;

  result.hits = lhs.hits - rhs.hits;
  result.misses = lhs.misses - rhs.misses;

  result.total_miss_latency_cycles = lhs.total_miss_latency_cycles - rhs.total_miss_latency_cycles;
  return result;
}
