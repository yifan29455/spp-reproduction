#include "bop.h"

#include <algorithm>
#include <iostream>

uint16_t bop::rr_tag(uint64_t line)
{
  return static_cast<uint16_t>((line >> 6U) & ((uint64_t{1} << RR_TAG_BITS) - 1U));
}

std::size_t bop::rr_index_left(uint64_t line)
{
  return static_cast<std::size_t>((line ^ (line >> 6U)) & (RR_SETS - 1U));
}

std::size_t bop::rr_index_right(uint64_t line)
{
  return static_cast<std::size_t>((line ^ (line >> 12U)) & (RR_SETS - 1U));
}

bool bop::same_page(uint64_t lhs, uint64_t rhs)
{
  return ((lhs ^ rhs) >> 6U) == 0;
}

uint32_t bop::cycle_now() const
{
  return static_cast<uint32_t>(intern_->current_time.time_since_epoch() / intern_->clock_period) & TIME_MASK;
}

void bop::rr_insert_left(uint64_t line)
{
  const auto index = rr_index_left(line);
  recent_tag[0][index] = rr_tag(line);
  recent_valid[0][index] = true;
}

void bop::rr_insert_right(uint64_t line)
{
  const auto index = rr_index_right(line);
  recent_tag[1][index] = rr_tag(line);
  recent_valid[1][index] = true;
}

bool bop::rr_hit(uint64_t line) const
{
  const auto tag = rr_tag(line);
  const auto left = rr_index_left(line);
  const auto right = rr_index_right(line);
  return (recent_valid[0][left] && recent_tag[0][left] == tag) || (recent_valid[1][right] && recent_tag[1][right] == tag);
}

void bop::delay_push(uint64_t line)
{
  if (delay_queue[delay_tail].valid) {
    rr_insert_left(delay_queue[delay_head].line);
    delay_queue[delay_head].valid = false;
    delay_head = (delay_head + 1) % DELAYQ_SIZE;
    ++delay_overflows;
  }
  delay_queue[delay_tail] = delay_entry{line, cycle_now(), true};
  delay_tail = (delay_tail + 1) % DELAYQ_SIZE;
  ++delay_pushes;
}

bool bop::delay_ready() const
{
  if (!delay_queue[delay_head].valid)
    return false;

  const auto issue_cycle = delay_queue[delay_head].cycle;
  const auto ready_cycle = (issue_cycle + DELAY_CYCLES) & TIME_MASK;
  const auto current = cycle_now();
  if (ready_cycle >= issue_cycle)
    return current < issue_cycle || current >= ready_cycle;
  return current < issue_cycle && current >= ready_cycle;
}

void bop::delay_pop()
{
  for (std::size_t i = 0; i < DELAYQ_SIZE && delay_ready(); ++i) {
    rr_insert_left(delay_queue[delay_head].line);
    delay_queue[delay_head].valid = false;
    delay_head = (delay_head + 1) % DELAYQ_SIZE;
    ++delay_pops;
  }
}

void bop::update_mshr_threshold()
{
  const auto mshr_size = static_cast<int>(intern_->get_mshr_size());
  const auto threshold_max = std::max(1, mshr_size - 4);
  const auto threshold_min = std::min(2, threshold_max);
  if (prefetch_score > LOW_SCORE || llc_rate > 2 * BANDWIDTH) {
    mshr_threshold = threshold_max;
  } else if (llc_rate < BANDWIDTH) {
    mshr_threshold = threshold_min;
  } else {
    mshr_threshold = threshold_min + (threshold_max - threshold_min) * (llc_rate - BANDWIDTH) / BANDWIDTH;
  }
}

void bop::llc_access()
{
  const auto current = cycle_now();
  const auto delta = static_cast<int>((current - last_cycle) & TIME_MASK);
  last_cycle = current;
  llc_rate_gauge += delta - llc_rate;
  if (llc_rate_gauge > GAUGE_MAX) {
    llc_rate_gauge = GAUGE_MAX;
    if (llc_rate < LLC_RATE_MAX) {
      ++llc_rate;
      update_mshr_threshold();
    }
  } else if (llc_rate_gauge < 0) {
    llc_rate_gauge = 0;
    if (llc_rate > 0) {
      --llc_rate;
      update_mshr_threshold();
    }
  }
}

void bop::reset_learning()
{
  scores.fill(0);
  candidate_index = 0;
  round = 0;
  max_score = 0;
  best_offset = 0;
}

void bop::learn(uint64_t line)
{
  ++learning_events;
  if (in_warmup())
    ++warmup_learning_events;
  else
    ++roi_learning_events;

  const int offset = OFFSETS[candidate_index];
  const auto target = static_cast<int64_t>(line) - offset;
  if (target >= 0 && same_page(static_cast<uint64_t>(target), line) && rr_hit(static_cast<uint64_t>(target))) {
    ++scores[candidate_index];
    ++rr_training_hits;
    if (scores[candidate_index] >= max_score) {
      max_score = scores[candidate_index];
      best_offset = offset;
    }
  }

  if (candidate_index == OFFSETS.size() - 1) {
    ++round;
    if (max_score >= static_cast<int>(SCORE_MAX) || round == ROUND_MAX) {
      prefetch_offset = best_offset != 0 ? best_offset : 1;
      prefetch_score = max_score;
      update_mshr_threshold();
      if (max_score <= BAD_SCORE)
        prefetch_offset = 0;
      reset_learning();
      return;
    }
  }
  candidate_index = (candidate_index + 1) % OFFSETS.size();
}

void bop::count_issued(bool fill_l2)
{
  ++issued;
  if (fill_l2)
    ++l2_issued;
  else
    ++llc_issued;
  if (in_warmup()) {
    ++warmup_issued;
    if (fill_l2)
      ++warmup_l2_issued;
    else
      ++warmup_llc_issued;
  } else {
    ++roi_issued;
    if (fill_l2)
      ++roi_l2_issued;
    else
      ++roi_llc_issued;
  }
}

bool bop::demand_merges_prefetch(uint64_t line) const
{
  const auto address = champsim::address{line << LOG2_BLOCK_SIZE};
  const auto matches = [address](const auto& entry) { return entry.type == access_type::PREFETCH && entry.address == address; };
  return std::any_of(intern_->MSHR.begin(), intern_->MSHR.end(), matches) || std::any_of(intern_->inflight_fills.begin(), intern_->inflight_fills.end(), matches);
}

void bop::count_useful(bool late)
{
  ++useful;
  if (late) {
    ++late_useful;
    if (in_warmup())
      ++warmup_late_useful;
    else
      ++roi_late_useful;
  }
  if (in_warmup())
    ++warmup_useful;
  else
    ++roi_useful;
}

void bop::issue(uint64_t line)
{
  if (prefetch_offset == 0) {
    delay_push(line);
    return;
  }

  const auto target_signed = static_cast<int64_t>(line) + prefetch_offset;
  if (target_signed < 0 || !same_page(line, static_cast<uint64_t>(target_signed))) {
    ++page_rejects;
    return;
  }

  ++issue_attempts;
  const auto target = static_cast<uint64_t>(target_signed);
  const bool duplicate_pending = pending_offsets.find(target) != pending_offsets.end();
  if (duplicate_pending)
    ++duplicate_attempts;

  const bool fill_l2 = intern_->get_mshr_occupancy() < static_cast<std::size_t>(mshr_threshold);
  if (!fill_l2 && prefetch_score <= LOW_SCORE)
    return;

  if (fill_l2)
    delay_push(line);

  const bool accepted = prefetch_line(champsim::address{target << LOG2_BLOCK_SIZE}, fill_l2, 0);
  if (!accepted) {
    ++failed;
    return;
  }

  count_issued(fill_l2);
  if (duplicate_pending)
    ++duplicate_issued;
  // Only requests filling this L2 can produce this module's fill callback.
  // LLC-only requests must not occupy the bounded L2 attribution table.
  if (fill_l2) {
    pending_offsets[target] = prefetch_offset;
    if (pending_offsets.size() > 8192) {
      pending_offsets.erase(pending_offsets.begin());
      ++pending_offset_evictions;
    }
  }
  llc_access();
}

void bop::prefetcher_initialize()
{
  for (auto& bank : recent_valid)
    bank.fill(false);
  for (auto& entry : delay_queue)
    entry = delay_entry{};
  pending_offsets.clear();
  delay_head = 0;
  delay_tail = 0;
  prefetch_offset = 1;
  prefetch_score = static_cast<int>(SCORE_MAX);
  llc_rate = 0;
  llc_rate_gauge = GAUGE_MAX / 2;
  last_cycle = 0;
  reset_learning();
  update_mshr_threshold();
  std::cout << "BOP candidates=" << OFFSETS.size() << " rounds=" << ROUND_MAX << " delay_q=" << DELAYQ_SIZE << " delay_cycles=" << DELAY_CYCLES
            << " mshr_threshold=" << mshr_threshold << std::endl;
}

uint32_t bop::prefetcher_cache_operate(champsim::address addr, champsim::address /*ip*/, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                       uint32_t metadata_in)
{
  if (type != access_type::LOAD && type != access_type::RFO && type != access_type::PREFETCH)
    return metadata_in;

  ++accesses;
  const auto line = champsim::block_number{addr}.to<uint64_t>();
  const bool late_useful_event = !cache_hit && type != access_type::PREFETCH && demand_merges_prefetch(line);
  if (useful_prefetch || late_useful_event)
    count_useful(late_useful_event);

  delay_pop();
  if (!cache_hit)
    llc_access();

  // DPC-2 learns and issues on an L2 miss or on a demand hit to a prefetched line.
  if (!cache_hit || useful_prefetch) {
    learn(line);
    issue(line);
  }
  return metadata_in;
}

uint32_t bop::prefetcher_cache_fill(champsim::address addr, long /*set*/, long /*way*/, uint8_t prefetch,
                                    champsim::address /*evicted_addr*/, uint32_t metadata_in)
{
  ++fill_events;
  const auto line = champsim::block_number{addr}.to<uint64_t>();
  const auto pending = pending_offsets.find(line);
  if (prefetch)
    ++prefetch_fill_events;
  if (pending != pending_offsets.end())
    pending_offsets.erase(pending);

  // Match DPC-2: every prefetched L2 fill updates the right RR bank. Bookkeeping
  // eviction must not suppress training; with no offset, demand fills seed it.
  if (prefetch || prefetch_offset == 0) {
    const auto baseline = static_cast<int64_t>(line) - prefetch_offset;
    if (baseline >= 0 && same_page(line, static_cast<uint64_t>(baseline)))
      rr_insert_right(static_cast<uint64_t>(baseline));
  }
  return metadata_in;
}

void bop::prefetcher_final_stats()
{
  std::cout << "BOP issued: " << issued << " filtered: " << filtered << " failed: " << failed << " useful: " << useful << std::endl;
  std::cout << "BOP_STATS accesses=" << accesses << " learning_events=" << learning_events << " rr_training_hits=" << rr_training_hits
            << " issue_attempts=" << issue_attempts << " issued=" << issued << " l2_issued=" << l2_issued << " llc_issued=" << llc_issued
            << " duplicate_attempts=" << duplicate_attempts << " duplicate_issued=" << duplicate_issued
            << " pending_offset_evictions=" << pending_offset_evictions
            << " filtered=" << filtered << " failed=" << failed << " useful=" << useful
            << " late_useful=" << late_useful << " warmup_late_useful=" << warmup_late_useful
            << " roi_late_useful=" << roi_late_useful
            << " fill_events=" << fill_events << " prefetch_fill_events=" << prefetch_fill_events << " delay_pushes=" << delay_pushes
            << " delay_pops=" << delay_pops << " delay_overflows=" << delay_overflows << " page_rejects=" << page_rejects
            << " warmup_issued=" << warmup_issued << " roi_issued=" << roi_issued << " warmup_useful=" << warmup_useful
            << " roi_useful=" << roi_useful << " warmup_l2_issued=" << warmup_l2_issued << " roi_l2_issued=" << roi_l2_issued
            << " warmup_llc_issued=" << warmup_llc_issued << " roi_llc_issued=" << roi_llc_issued
            << " warmup_learning_events=" << warmup_learning_events
            << " roi_learning_events=" << roi_learning_events << " final_offset=" << prefetch_offset << " final_score=" << prefetch_score
            << " final_mshr_threshold=" << mshr_threshold << " llc_rate=" << llc_rate << std::endl;
}
