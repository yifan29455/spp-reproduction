#ifndef SPP_H
#define SPP_H

#include <cstdint>
#include <vector>

#ifndef SPP_LOOKAHEAD_ENABLED
#define SPP_LOOKAHEAD_ENABLED 1
#endif
#ifndef SPP_GHR_ENABLED
#define SPP_GHR_ENABLED 1
#endif
#ifndef SPP_PREFETCH_THRESHOLD
#define SPP_PREFETCH_THRESHOLD 25
#endif
#ifndef SPP_LOOKAHEAD_MAX_DEPTH
#define SPP_LOOKAHEAD_MAX_DEPTH 32
#endif
#ifndef SPP_GHR_DENOM_PLUS_ONE
#define SPP_GHR_DENOM_PLUS_ONE 0
#endif
#ifndef SPP_GHR_ALPHA_CLAMP_99
#define SPP_GHR_ALPHA_CLAMP_99 1
#endif
#ifndef SPP_FILTER_EVICTION_POLICY
// The paper's C_useful is an event counter. L2 eviction clears the filter
// entry but does not undo a previously counted useful-prefetch event.
// 0: historical upstream behavior (decrement on unused eviction)
// 1: former local behavior (decrement on useful eviction; diagnostic only)
// 2: event-count semantics (no counter decrement on eviction)
#define SPP_FILTER_EVICTION_POLICY 2
#endif
#ifndef SPP_ROLLBACK_REJECTED_L2
#define SPP_ROLLBACK_REJECTED_L2 1
#endif
#ifndef SPP_GHR_COUNT_ACCEPTED_L2
#define SPP_GHR_COUNT_ACCEPTED_L2 1
#endif

#include "spp_filter_semantics.h"
#include "cache.h"
#include "modules.h"
#include "msl/lru_table.h"

struct spp_dev : public champsim::modules::prefetcher {

  // SPP functional knobs
  constexpr static bool LOOKAHEAD_ON = (SPP_LOOKAHEAD_ENABLED != 0);
  constexpr static bool FILTER_ON = true;
  constexpr static bool GHR_ON = (SPP_GHR_ENABLED != 0);
  constexpr static bool SPP_SANITY_CHECK = true;
  constexpr static bool SPP_DEBUG_PRINT = false;

  // Signature table parameters
  constexpr static std::size_t ST_SET = 1;
  constexpr static std::size_t ST_WAY = 256;
  constexpr static unsigned ST_TAG_BIT = 16;
  constexpr static unsigned SIG_SHIFT = 3;
  constexpr static unsigned SIG_BIT = 12;
  constexpr static uint32_t SIG_MASK = ((1 << SIG_BIT) - 1);
  constexpr static unsigned SIG_DELTA_BIT = 7;

  // Pattern table parameters
  constexpr static std::size_t PT_SET = 512;
  constexpr static std::size_t PT_WAY = 4;
  constexpr static unsigned C_SIG_BIT = 4;
  constexpr static unsigned C_DELTA_BIT = 4;
  constexpr static uint32_t C_SIG_MAX = ((1 << C_SIG_BIT) - 1);
  constexpr static uint32_t C_DELTA_MAX = ((1 << C_DELTA_BIT) - 1);

  // Prefetch filter parameters
  constexpr static unsigned QUOTIENT_BIT = 10;
  constexpr static unsigned REMAINDER_BIT = 6;
  constexpr static unsigned HASH_BIT = (QUOTIENT_BIT + REMAINDER_BIT + 1);
  constexpr static std::size_t FILTER_SET = (1 << QUOTIENT_BIT);
  constexpr static uint32_t FILL_THRESHOLD = 90;
  constexpr static uint32_t PF_THRESHOLD = SPP_PREFETCH_THRESHOLD;

  // Global register parameters
  constexpr static unsigned GLOBAL_COUNTER_BIT = 10;
  constexpr static uint32_t GLOBAL_COUNTER_MAX = ((1 << GLOBAL_COUNTER_BIT) - 1);
  constexpr static std::size_t MAX_GHR_ENTRY = 8;
  constexpr static std::size_t LOOKAHEAD_MAX_DEPTH = SPP_LOOKAHEAD_MAX_DEPTH;

  using prefetcher::prefetcher;
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, bool evicted_valid, uint32_t metadata_in);

  void prefetcher_initialize();
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();

  enum FILTER_REQUEST { SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT }; // Request type for prefetch filter
  static uint64_t get_hash(uint64_t key);

  struct block_in_page_extent : champsim::dynamic_extent {
    block_in_page_extent() : dynamic_extent(champsim::data::bits{LOG2_PAGE_SIZE}, champsim::data::bits{LOG2_BLOCK_SIZE}) {}
  };
  using offset_type = champsim::address_slice<block_in_page_extent>;

  class SIGNATURE_TABLE
  {
    struct tag_extent : champsim::dynamic_extent {
      tag_extent() : dynamic_extent(champsim::data::bits{ST_TAG_BIT + LOG2_PAGE_SIZE}, champsim::data::bits{LOG2_PAGE_SIZE}) {}
    };

  public:
    spp_dev* _parent;
    using tag_type = champsim::address_slice<tag_extent>;

    bool valid[ST_SET][ST_WAY];
    tag_type tag[ST_SET][ST_WAY];
    offset_type last_offset[ST_SET][ST_WAY];
    uint32_t sig[ST_SET][ST_WAY], lru[ST_SET][ST_WAY];

    SIGNATURE_TABLE()
    {
      for (uint32_t set = 0; set < ST_SET; set++)
        for (uint32_t way = 0; way < ST_WAY; way++) {
          valid[set][way] = 0;
          tag[set][way] = tag_type{};
          last_offset[set][way] = offset_type{};
          sig[set][way] = 0;
          lru[set][way] = way;
        }
    };

    void read_and_update_sig(champsim::address addr, uint32_t& last_sig, uint32_t& curr_sig, typename offset_type::difference_type& delta);
  };

  class PATTERN_TABLE
  {
  public:
    spp_dev* _parent;
    typename offset_type::difference_type delta[PT_SET][PT_WAY];
    uint32_t c_delta[PT_SET][PT_WAY], c_sig[PT_SET];

    PATTERN_TABLE()
    {
      for (uint32_t set = 0; set < PT_SET; set++) {
        for (uint32_t way = 0; way < PT_WAY; way++) {
          delta[set][way] = 0;
          c_delta[set][way] = 0;
        }
        c_sig[set] = 0;
      }
    }

    void update_pattern(uint32_t last_sig, typename offset_type::difference_type curr_delta);
    void read_pattern(uint32_t curr_sig, std::vector<typename offset_type::difference_type>& prefetch_delta, std::vector<uint32_t>& confidence_q,
                      uint32_t& lookahead_way, uint32_t& lookahead_conf, uint32_t& pf_q_tail, uint32_t& depth);
  };

  class PREFETCH_FILTER
  {
  public:
    spp_dev* _parent;
    uint64_t remainder_tag[FILTER_SET];
    bool valid[FILTER_SET], // Consider this as "prefetched"
        useful[FILTER_SET]; // Consider this as "used"

    PREFETCH_FILTER()
    {
      for (uint32_t set = 0; set < FILTER_SET; set++) {
        remainder_tag[set] = 0;
        valid[set] = 0;
        useful[set] = 0;
      }
    }

    bool check(champsim::address pf_addr, FILTER_REQUEST filter_request);
  };

  class GLOBAL_REGISTER
  {
  public:
    spp_dev* _parent;
    // Global counters to calculate global prefetching accuracy
    uint32_t pf_useful, pf_issued;
    uint32_t global_accuracy; // Alpha value in Section III. Equation 3

    // Global History Register (GHR) entries
    uint8_t valid[MAX_GHR_ENTRY];
    uint32_t sig[MAX_GHR_ENTRY], confidence[MAX_GHR_ENTRY];
    offset_type offset[MAX_GHR_ENTRY];
    typename offset_type::difference_type delta[MAX_GHR_ENTRY];

    GLOBAL_REGISTER()
    {
      pf_useful = 0;
      pf_issued = 0;
      global_accuracy = 0;

      for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
        valid[i] = 0;
        sig[i] = 0;
        confidence[i] = 0;
        offset[i] = offset_type{};
        delta[i] = 0;
      }
    }

    void update_entry(uint32_t pf_sig, uint32_t pf_confidence, offset_type pf_offset, typename offset_type::difference_type pf_delta);
    uint32_t check_entry(offset_type page_offset);
  };

  uint64_t lookahead_events = 0;
  uint64_t lookahead_depth_sum = 0;
  uint64_t lookahead_depth_max = 0;
  uint64_t lookahead_depth_max_warmup = 0;
  uint64_t lookahead_depth_max_roi = 0;
  uint64_t lookahead_depth_limit_hits = 0;
  uint64_t ghr_lookups = 0;
  uint64_t ghr_hits = 0;
  uint64_t cross_page_candidates = 0;
  uint64_t lookahead_queue_growths = 0;
  uint64_t lookahead_queue_growths_warmup = 0;
  uint64_t lookahead_queue_growths_roi = 0;
  uint64_t l2_prefetches_issued = 0;
  uint64_t llc_prefetches_issued = 0;
  uint64_t l2_prefetches_issued_warmup = 0;
  uint64_t l2_prefetches_issued_roi = 0;
  uint64_t llc_prefetches_issued_warmup = 0;
  uint64_t llc_prefetches_issued_roi = 0;
  uint64_t l2_prefetch_rejections = 0;
  uint64_t l2_prefetch_rejections_warmup = 0;
  uint64_t l2_prefetch_rejections_roi = 0;
  uint64_t ghr_alpha_over_100_events = 0;
  uint32_t ghr_alpha_max = 0;

  uint64_t lookahead_events_warmup = 0;
  uint64_t lookahead_events_roi = 0;
  uint64_t lookahead_depth_sum_warmup = 0;
  uint64_t lookahead_depth_sum_roi = 0;
  uint64_t lookahead_depth_limit_hits_warmup = 0;
  uint64_t lookahead_depth_limit_hits_roi = 0;
  uint64_t ghr_lookups_warmup = 0;
  uint64_t ghr_lookups_roi = 0;
  uint64_t ghr_hits_warmup = 0;
  uint64_t ghr_hits_roi = 0;
  uint64_t cross_page_candidates_warmup = 0;
  uint64_t cross_page_candidates_roi = 0;

  void record_ghr_lookup(bool hit);
  void record_cross_page_candidate();

  SIGNATURE_TABLE ST;
  PATTERN_TABLE PT;
  PREFETCH_FILTER FILTER;
  GLOBAL_REGISTER GHR;
};

#endif
