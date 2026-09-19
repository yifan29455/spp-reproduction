/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <utility>
#include <nlohmann/json.hpp>

#include "stats_printer.h"

void to_json(nlohmann::json& j, const O3_CPU::stats_type& stats)
{
  constexpr std::array types{branch_type::BRANCH_DIRECT_JUMP, branch_type::BRANCH_INDIRECT,      branch_type::BRANCH_CONDITIONAL,
                             branch_type::BRANCH_DIRECT_CALL, branch_type::BRANCH_INDIRECT_CALL, branch_type::BRANCH_RETURN};

  auto total_mispredictions = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0LL, [btm = stats.branch_type_misses](auto acc, auto next) { return acc + btm.value_or(next, 0); }));

  std::map<std::string, std::size_t> mpki{};
  for (auto type : types) {
    mpki.emplace(branch_type_names.at(champsim::to_underlying(type)), stats.branch_type_misses.value_or(type, 0));
  }

  j = nlohmann::json{{"instructions", stats.instrs()},
                     {"cycles", stats.cycles()},
                     {"Avg ROB occupancy at mispredict", std::ceil(stats.total_rob_occupancy_at_branch_mispredict) / std::ceil(total_mispredictions)},
                     {"mispredict", mpki}};
}

void to_json(nlohmann::json& j, const CACHE::stats_type& stats)
{
  using hits_value_type = typename decltype(stats.hits)::value_type;
  using misses_value_type = typename decltype(stats.misses)::value_type;
  using miss_merge_value_type = typename decltype(stats.miss_merge)::value_type;
  using fill_value_type = typename decltype(stats.fill)::value_type;

  std::map<std::string, nlohmann::json> statsmap;
  statsmap.emplace("prefetch requested", stats.pf_requested);
  statsmap.emplace("prefetch issued", stats.pf_issued);
  statsmap.emplace("useful prefetch", stats.pf_useful);
  statsmap.emplace("late useful prefetch", stats.pf_late_useful);
  statsmap.emplace("useless prefetch", stats.pf_useless);
  statsmap.emplace("prefetch fill", stats.pf_fill);
  statsmap.emplace("upstream prefetch received", stats.pf_upstream_received);
  statsmap.emplace("prefetch attempt local PQ rejected", stats.pf_attempt_pq_rejected);
  statsmap.emplace("prefetch attempt hit existing", stats.pf_attempt_hit_existing);
  statsmap.emplace("prefetch attempt merged with prefetch", stats.pf_attempt_merged_prefetch);
  statsmap.emplace("prefetch attempt merged with demand", stats.pf_attempt_merged_demand);
  statsmap.emplace("prefetch attempt MSHR rejected", stats.pf_attempt_mshr_rejected);
  statsmap.emplace("prefetch attempt lower queue rejected", stats.pf_attempt_lower_queue_rejected);
  statsmap.emplace("prefetch attempt accepted without local fill", stats.pf_attempt_no_local_fill);
  statsmap.emplace("ROI prefetch cohort created", stats.pf_roi_cohort_created);
  statsmap.emplace("ROI prefetch cohort timely useful", stats.pf_roi_cohort_timely);
  statsmap.emplace("ROI prefetch cohort late useful", stats.pf_roi_cohort_late);
  statsmap.emplace("ROI prefetch cohort unused", stats.pf_roi_cohort_unused);
  statsmap.emplace("ROI prefetch cohort unused by eviction", stats.pf_roi_cohort_unused_evicted);
  statsmap.emplace("ROI prefetch cohort unused by invalidation", stats.pf_roi_cohort_unused_invalidated);
  statsmap.emplace("ROI prefetch cohort unused by non-demand access", stats.pf_roi_cohort_unused_other);
  statsmap.emplace("ROI prefetch cohort unresolved at end", stats.pf_roi_cohort_unresolved_end);
  statsmap.emplace("ROI prefetch cohort balance error", stats.pf_roi_cohort_balance_error);
  statsmap.emplace("prefetch request sequence count", stats.pf_request_seq_count);
  statsmap.emplace("prefetch request sequence hash", stats.pf_request_seq_hash);
  statsmap.emplace("warmup prefetch request sequence count", stats.pf_warmup_request_seq_count);
  statsmap.emplace("warmup prefetch request sequence hash", stats.pf_warmup_request_seq_hash);
  statsmap.emplace("warmup-origin active prefetch transactions at ROI start", stats.pf_warmup_active_transactions_roi_start);
  statsmap.emplace("warmup-origin pending prefetch requests at ROI start", stats.pf_warmup_pending_requests_roi_start);
  statsmap.emplace("warmup-origin prefetch transactions created in ROI", stats.pf_warmup_transactions_created_roi);
  statsmap.emplace("warmup-origin timely useful in ROI", stats.pf_warmup_timely_roi);
  statsmap.emplace("warmup-origin late useful in ROI", stats.pf_warmup_late_roi);
  statsmap.emplace("warmup-origin unused in ROI", stats.pf_warmup_unused_roi);
  statsmap.emplace("warmup-origin unused by eviction in ROI", stats.pf_warmup_unused_evicted_roi);
  statsmap.emplace("warmup-origin unused by invalidation in ROI", stats.pf_warmup_unused_invalidated_roi);
  statsmap.emplace("warmup-origin unused by non-demand access in ROI", stats.pf_warmup_unused_other_roi);
  statsmap.emplace("warmup-origin pending prefetch requests at ROI end", stats.pf_warmup_pending_requests_roi_end);
  statsmap.emplace("warmup-origin unresolved transactions at ROI end", stats.pf_warmup_unresolved_roi_end);
  statsmap.emplace("warmup-origin cohort balance error in ROI", stats.pf_warmup_cohort_balance_error_roi);
  statsmap.emplace("ROI-origin pending prefetch requests at ROI end", stats.pf_roi_pending_requests_roi_end);

  uint64_t total_downstream_demands = stats.fill.total();
  for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu)
    total_downstream_demands -= stats.fill.value_or(std::pair{access_type::PREFETCH, cpu}, fill_value_type{});

  statsmap.emplace("miss latency", std::ceil(stats.total_miss_latency_cycles) / std::ceil(total_downstream_demands));
  for (const auto type : {access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}) {
    std::vector<hits_value_type> hits;
    std::vector<misses_value_type> misses;
    std::vector<miss_merge_value_type> miss_merges;

    for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
      hits.push_back(stats.hits.value_or(std::pair{type, cpu}, hits_value_type{}));
      misses.push_back(stats.misses.value_or(std::pair{type, cpu}, misses_value_type{}));
      miss_merges.push_back(stats.miss_merge.value_or(std::pair{type, cpu}, miss_merge_value_type{}));
    }

    statsmap.emplace(access_type_names.at(champsim::to_underlying(type)), nlohmann::json{{"hit", hits}, {"miss", misses}, {"miss_merge", miss_merges}});
  }

  j = statsmap;
}

void to_json(nlohmann::json& j, const DRAM_CHANNEL::stats_type stats)
{
  j = nlohmann::json{{"RQ ROW_BUFFER_HIT", stats.RQ_ROW_BUFFER_HIT},
                     {"RQ ROW_BUFFER_MISS", stats.RQ_ROW_BUFFER_MISS},
                     {"WQ ROW_BUFFER_HIT", stats.WQ_ROW_BUFFER_HIT},
                     {"WQ ROW_BUFFER_MISS", stats.WQ_ROW_BUFFER_MISS},
                     {"AVG DBUS CONGESTED CYCLE", (std::ceil(stats.dbus_cycle_congested) / std::ceil(stats.dbus_count_congested))},
                     {"REFRESHES ISSUED", stats.refresh_cycles}};
}

namespace champsim
{
void to_json(nlohmann::json& j, const champsim::phase_stats stats)
{
  std::map<std::string, nlohmann::json> roi_stats;
  roi_stats.emplace("cores", stats.roi_cpu_stats);
  roi_stats.emplace("DRAM", stats.roi_dram_stats);
  for (auto x : stats.roi_cache_stats) {
    roi_stats.emplace(x.name, x);
  }

  std::map<std::string, nlohmann::json> sim_stats;
  sim_stats.emplace("cores", stats.sim_cpu_stats);
  sim_stats.emplace("DRAM", stats.sim_dram_stats);
  for (auto x : stats.sim_cache_stats) {
    sim_stats.emplace(x.name, x);
  }

  std::map<std::string, nlohmann::json> statsmap{{"name", stats.name}, {"traces", stats.trace_names}};
  statsmap.emplace("roi", roi_stats);
  statsmap.emplace("sim", sim_stats);
  j = statsmap;
}
} // namespace champsim

void champsim::json_printer::print(std::vector<phase_stats>& stats) { stream << nlohmann::json::array_t{std::begin(stats), std::end(stats)}; }
