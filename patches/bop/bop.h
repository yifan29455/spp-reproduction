#ifndef PREFETCHER_BOP_H
#define PREFETCHER_BOP_H

#include <array>
#include <cstdint>
#include <unordered_map>

#include "cache.h"
#include "champsim.h"
#include "modules.h"

// Modern-ChampSim adapter for Pierre Michaud's DPC-2 Best-Offset prefetcher.
// DPC-2's simulator-specific callbacks are mapped to the current cache module API.
struct bop : public champsim::modules::prefetcher {
  using prefetcher::prefetcher;

  static constexpr std::array<int, 46> OFFSETS{
      1,  -1, 2,  -2, 3,  -3, 4,  -4, 5,  -5, 6,  -6, 7,  -7, 8,  -8, 9,  -9, 10, -10, 11, -11, 12,
      -12, 13, -13, 14, -14, 15, -15, 16, -16, 18, -18, 20, -20, 24, -24, 30, -30, 32, -32, 36, -36, 40, -40};

  static constexpr std::size_t RR_SETS = 64;
  static constexpr std::size_t RR_TAG_BITS = 12;
  static constexpr std::size_t ROUND_MAX = 100;
  static constexpr std::size_t SCORE_MAX = 31;
  static constexpr std::size_t DELAYQ_SIZE = 15;
  static constexpr uint32_t DELAY_CYCLES = 60;
  static constexpr uint32_t TIME_MASK = (1U << 12U) - 1U;
  static constexpr int LOW_SCORE = 20;
  static constexpr int BAD_SCORE = 1;
  static constexpr int BANDWIDTH = 16;
  static constexpr int GAUGE_MAX = 8191;
  static constexpr int LLC_RATE_MAX = 255;

  struct delay_entry {
    uint64_t line = 0;
    uint32_t cycle = 0;
    bool valid = false;
  };

  std::array<std::array<uint16_t, RR_SETS>, 2> recent_tag{};
  std::array<std::array<bool, RR_SETS>, 2> recent_valid{};
  std::array<delay_entry, DELAYQ_SIZE> delay_queue{};
  std::array<int, OFFSETS.size()> scores{};
  std::unordered_map<uint64_t, int> pending_offsets;

  std::size_t candidate_index = 0;
  std::size_t round = 0;
  int max_score = 0;
  int best_offset = 0;
  int prefetch_offset = 1;
  int prefetch_score = static_cast<int>(SCORE_MAX);
  int mshr_threshold = 2;
  int llc_rate = 0;
  int llc_rate_gauge = GAUGE_MAX / 2;
  uint32_t last_cycle = 0;
  std::size_t delay_head = 0;
  std::size_t delay_tail = 0;

  uint64_t accesses = 0;
  uint64_t learning_events = 0;
  uint64_t rr_training_hits = 0;
  uint64_t issue_attempts = 0;
  uint64_t issued = 0;
  uint64_t l2_issued = 0;
  uint64_t llc_issued = 0;
  uint64_t filtered = 0;
  uint64_t duplicate_attempts = 0;
  uint64_t duplicate_issued = 0;
  uint64_t pending_offset_evictions = 0;
  uint64_t failed = 0;
  uint64_t useful = 0;
  uint64_t late_useful = 0;
  uint64_t warmup_late_useful = 0;
  uint64_t roi_late_useful = 0;
  uint64_t fill_events = 0;
  uint64_t prefetch_fill_events = 0;
  uint64_t delay_pushes = 0;
  uint64_t delay_pops = 0;
  uint64_t delay_overflows = 0;
  uint64_t page_rejects = 0;
  uint64_t warmup_issued = 0;
  uint64_t roi_issued = 0;
  uint64_t warmup_l2_issued = 0;
  uint64_t roi_l2_issued = 0;
  uint64_t warmup_llc_issued = 0;
  uint64_t roi_llc_issued = 0;
  uint64_t warmup_useful = 0;
  uint64_t roi_useful = 0;
  uint64_t warmup_learning_events = 0;
  uint64_t roi_learning_events = 0;

  static uint16_t rr_tag(uint64_t line);
  static std::size_t rr_index_left(uint64_t line);
  static std::size_t rr_index_right(uint64_t line);
  static bool same_page(uint64_t lhs, uint64_t rhs);

  uint32_t cycle_now() const;
  bool in_warmup() const { return intern_->warmup; }
  void rr_insert_left(uint64_t line);
  void rr_insert_right(uint64_t line);
  bool rr_hit(uint64_t line) const;
  void delay_push(uint64_t line);
  bool delay_ready() const;
  void delay_pop();
  void update_mshr_threshold();
  void llc_access();
  void reset_learning();
  void learn(uint64_t line);
  void issue(uint64_t line);
  void count_issued(bool fill_l2);
  bool demand_merges_prefetch(uint64_t line) const;
  void count_useful(bool late);

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr,
                                 uint32_t metadata_in);
  void prefetcher_initialize();
  void prefetcher_final_stats();
};

#endif
