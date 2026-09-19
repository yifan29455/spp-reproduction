#ifndef SPP_FILTER_SEMANTICS_H
#define SPP_FILTER_SEMANTICS_H

#include <cstdint>

namespace spp_filter_semantics
{
constexpr bool tag_matches(uint64_t stored, uint64_t requested) { return stored == requested; }

constexpr bool entry_matches(bool valid, bool useful, uint64_t stored, uint64_t requested)
{
  return (valid || useful) && tag_matches(stored, requested);
}

constexpr bool demand_is_first_use(bool valid, bool useful, uint64_t stored, uint64_t requested)
{
  return valid && !useful && tag_matches(stored, requested);
}

constexpr bool should_clear_on_eviction(bool valid, bool useful, uint64_t stored, uint64_t requested)
{
  return entry_matches(valid, useful, stored, requested);
}
} // namespace spp_filter_semantics

#endif // SPP_FILTER_SEMANTICS_H
