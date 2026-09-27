#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <limits>
#include <mach/mach_time.h>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" std::uint64_t transfer_sme_vl_bytes();
#define DECLARE_TRANSFER_LOAD(W, G) \
  extern "C" std::uint64_t transfer_sme_load_##W##_g##G(const std::uint8_t*, std::uint64_t, std::uint64_t);
#define DECLARE_TRANSFER_STORE(W, G) \
  extern "C" std::uint64_t transfer_sme_store_##W##_g##G(std::uint8_t*, std::uint64_t, std::uint64_t, std::uint64_t);
#define DECLARE_TRANSFER_WIDTH(W) \
  DECLARE_TRANSFER_LOAD(W, 1) DECLARE_TRANSFER_LOAD(W, 2) DECLARE_TRANSFER_LOAD(W, 4) \
  DECLARE_TRANSFER_STORE(W, 1) DECLARE_TRANSFER_STORE(W, 2) DECLARE_TRANSFER_STORE(W, 4)
DECLARE_TRANSFER_WIDTH(b)
DECLARE_TRANSFER_WIDTH(h)
DECLARE_TRANSFER_WIDTH(w)
DECLARE_TRANSFER_WIDTH(d)
#undef DECLARE_TRANSFER_WIDTH
#undef DECLARE_TRANSFER_STORE
#undef DECLARE_TRANSFER_LOAD

namespace {

constexpr std::uint64_t kAlignment = 128;
constexpr std::uint64_t kGuardBytes = 128;
constexpr std::uint64_t kMinimumFootprint = 2 * 1024;
constexpr std::uint64_t kMaximumFootprint = 512ULL * 1024ULL * 1024ULL;
std::uint64_t calibrated_vector_length_bytes = 0;
std::uint64_t kFrozenIterations = 0;
constexpr std::uint64_t kFrozenSeed = 909001;
constexpr std::uint64_t kSessions = 3;
constexpr std::uint64_t kWarmups = 2;
constexpr std::uint64_t kRecorded = 5;
constexpr std::uint64_t kInterferenceFootprint = 2ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kCpuStride = 64;
constexpr std::uint64_t kCpuOuter = 16;
constexpr std::uint64_t kStoreTag = 0x1122334455667788ULL;

using LoadFn = std::uint64_t (*)(const std::uint8_t*, std::uint64_t, std::uint64_t);
using StoreFn = std::uint64_t (*)(std::uint8_t*, std::uint64_t, std::uint64_t, std::uint64_t);

[[noreturn]] void fail(const std::string& text) { throw std::runtime_error(text); }
void require(bool condition, const std::string& text) { if (!condition) fail(text); }

bool valid_group(std::uint64_t group) { return group == 1 || group == 2 || group == 4; }

bool valid_footprint(std::uint64_t footprint) {
  return footprint >= kMinimumFootprint && footprint <= kMaximumFootprint &&
         (footprint & (footprint - 1)) == 0;
}

std::uint64_t checked_mul(std::uint64_t left, std::uint64_t right, const std::string& name) {
  require(left == 0 || right <= std::numeric_limits<std::uint64_t>::max() / left, name + " overflow");
  return left * right;
}

std::uint64_t parse_decimal(const char* text, const std::string& name) {
  require(text != nullptr && *text != '\0', name + " requires decimal digits");
  std::uint64_t value = 0;
  for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor; ++cursor) {
    require(std::isdigit(*cursor) != 0, name + " requires decimal digits");
    const std::uint64_t digit = static_cast<std::uint64_t>(*cursor - static_cast<unsigned char>('0'));
    require(value <= (std::numeric_limits<std::uint64_t>::max() - digit) / 10, name + " overflow");
    value = value * 10 + digit;
  }
  return value;
}

char parse_width(const char* text) {
  require(text != nullptr && text[0] != '\0' && text[1] == '\0', "width must be B, H, W, or D");
  const char width = text[0];
  require(width == 'B' || width == 'H' || width == 'W' || width == 'D', "width must be B, H, W, or D");
  return width;
}

std::uint64_t element_bytes(char width) {
  switch (width) {
    case 'B': return 1;
    case 'H': return 2;
    case 'W': return 4;
    case 'D': return 8;
  }
  fail("unknown width");
}

LoadFn load_for(char width, std::uint64_t group) {
  switch (width) {
    case 'B': return group == 1 ? transfer_sme_load_b_g1 : group == 2 ? transfer_sme_load_b_g2 : group == 4 ? transfer_sme_load_b_g4 : nullptr;
    case 'H': return group == 1 ? transfer_sme_load_h_g1 : group == 2 ? transfer_sme_load_h_g2 : group == 4 ? transfer_sme_load_h_g4 : nullptr;
    case 'W': return group == 1 ? transfer_sme_load_w_g1 : group == 2 ? transfer_sme_load_w_g2 : group == 4 ? transfer_sme_load_w_g4 : nullptr;
    case 'D': return group == 1 ? transfer_sme_load_d_g1 : group == 2 ? transfer_sme_load_d_g2 : group == 4 ? transfer_sme_load_d_g4 : nullptr;
  }
  return nullptr;
}

StoreFn store_for(char width, std::uint64_t group) {
  switch (width) {
    case 'B': return group == 1 ? transfer_sme_store_b_g1 : group == 2 ? transfer_sme_store_b_g2 : group == 4 ? transfer_sme_store_b_g4 : nullptr;
    case 'H': return group == 1 ? transfer_sme_store_h_g1 : group == 2 ? transfer_sme_store_h_g2 : group == 4 ? transfer_sme_store_h_g4 : nullptr;
    case 'W': return group == 1 ? transfer_sme_store_w_g1 : group == 2 ? transfer_sme_store_w_g2 : group == 4 ? transfer_sme_store_w_g4 : nullptr;
    case 'D': return group == 1 ? transfer_sme_store_d_g1 : group == 2 ? transfer_sme_store_d_g2 : group == 4 ? transfer_sme_store_d_g4 : nullptr;
  }
  return nullptr;
}

struct Region {
  std::uint8_t* raw = nullptr;
  std::uint8_t* data = nullptr;
  std::uint64_t footprint = 0;

  explicit Region(std::uint64_t bytes) : footprint(bytes) {
    require(valid_footprint(bytes), "invalid native footprint");
    void* allocation = nullptr;
    require(posix_memalign(&allocation, kAlignment, static_cast<std::size_t>(bytes + 2 * kGuardBytes)) == 0 && allocation,
            "aligned allocation");
    raw = static_cast<std::uint8_t*>(allocation);
    data = raw + kGuardBytes;
    require(reinterpret_cast<std::uintptr_t>(data) % kAlignment == 0, "aligned interior buffer");
    std::memset(raw, 0xA5, kGuardBytes);
    std::memset(data + footprint, 0x5A, kGuardBytes);
  }

  Region(const Region&) = delete;
  Region& operator=(const Region&) = delete;
  ~Region() { std::free(raw); }

  void fill() {
    for (std::uint64_t index = 0; index < footprint; ++index)
      data[index] = static_cast<std::uint8_t>((17 * index + 3) & 0xffU);
  }

  bool guards_intact() const {
    for (std::uint64_t index = 0; index < kGuardBytes; ++index) {
      if (raw[index] != 0xA5 || data[footprint + index] != 0x5A) return false;
    }
    return true;
  }
};

struct Cell {
  std::string kind;
  char width = 'B';
  std::uint64_t group = 1;
  std::uint64_t footprint = kMinimumFootprint;
};

void validate_leaf_call(const std::uint8_t* base, std::uint64_t footprint, std::uint64_t iterations,
                        std::uint64_t group, std::uint64_t vector_length) {
  require(base != nullptr && iterations == kFrozenIterations && valid_group(group) &&
              vector_length == calibrated_vector_length_bytes,
          "native leaf precondition");
  const std::uint64_t stride = checked_mul(group, vector_length, "leaf stride");
  require(valid_footprint(footprint) && footprint >= stride && footprint % stride == 0,
          "integral native wrap precondition");
  const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
  require(address % kAlignment == 0 && address <= std::numeric_limits<std::uintptr_t>::max() - footprint,
          "native address/alignment/overflow precondition");
}

std::uint64_t requested_payload_bytes(std::uint64_t group, std::uint64_t vector_length) {
  return checked_mul(kFrozenIterations, checked_mul(group, vector_length, "requested stride"), "requested payload");
}

std::uint64_t touched_bytes(std::uint64_t footprint, std::uint64_t group, std::uint64_t vector_length) {
  return std::min(footprint, requested_payload_bytes(group, vector_length));
}

std::uint64_t full_traversal_iterations(std::uint64_t footprint, std::uint64_t group, std::uint64_t vector_length) {
  const std::uint64_t stride = checked_mul(group, vector_length, "full traversal stride");
  require(footprint % stride == 0, "full traversal must be integral");
  return footprint / stride;
}

std::uint64_t load_checksum_expected(const std::uint8_t* base, std::uint64_t footprint, std::uint64_t iterations,
                                     std::uint64_t group, std::uint64_t vector_length) {
  validate_leaf_call(base, footprint, iterations, group, vector_length);
  const std::uint64_t stride = checked_mul(group, vector_length, "load checksum stride");
  const std::uint64_t lanes = vector_length / 8;
  std::vector<std::uint64_t> accumulators(static_cast<std::size_t>(checked_mul(group, lanes, "checksum lanes")), 0);
  for (std::uint64_t iteration = 0, cursor = 0; iteration < iterations; ++iteration) {
    for (std::uint64_t z_group = 0; z_group < group; ++z_group) {
      for (std::uint64_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t word = 0;
        std::memcpy(&word, base + cursor + z_group * vector_length + lane * 8, sizeof(word));
        accumulators[static_cast<std::size_t>(z_group * lanes + lane)] += word;
      }
    }
    cursor += stride;
    if (cursor == footprint) cursor = 0;
  }
  std::vector<std::uint64_t> reductions(static_cast<std::size_t>(group), 0);
  for (std::uint64_t z_group = 0; z_group < group; ++z_group) {
    for (std::uint64_t lane = 0; lane < lanes; ++lane)
      reductions[static_cast<std::size_t>(z_group)] += accumulators[static_cast<std::size_t>(z_group * lanes + lane)];
  }
  auto ror64 = [](std::uint64_t value, unsigned bits) {
    return (value >> bits) | (value << (64U - bits));
  };
  std::uint64_t reduced = reductions[0];
  for (std::uint64_t z_group = 1; z_group < group; ++z_group)
    reduced ^= ror64(reductions[static_cast<std::size_t>(z_group)], static_cast<unsigned>(13 * z_group));
  return reduced ^ iterations;
}

std::uint64_t store_checksum_expected(std::uint64_t footprint, std::uint64_t iterations,
                                      std::uint64_t group, std::uint64_t tag) {
  return tag ^ iterations ^ footprint ^ group;
}

struct SuffixSamples {
  bool checked = false;
  std::uint64_t first_offset = 0;
  std::uint64_t last_offset = 0;
  std::uint8_t first_value = 0;
  std::uint8_t last_value = 0;
};

SuffixSamples capture_untouched_suffix_samples(const Region& region, std::uint64_t touched) {
  SuffixSamples samples;
  if (touched == region.footprint) return samples;
  samples.checked = true;
  samples.first_offset = touched;
  samples.last_offset = region.footprint - 1;
  samples.first_value = region.data[samples.first_offset];
  samples.last_value = region.data[samples.last_offset];
  return samples;
}

bool suffix_samples_intact(const Region& region, const SuffixSamples& samples) {
  return !samples.checked ||
         (region.data[samples.first_offset] == samples.first_value && region.data[samples.last_offset] == samples.last_value);
}

// Verify exactly the byte range the calibrated iteration count leaf can have written.  A
// larger configured capacity is retained as F, but is never reported as fully
// traversed unless I*G*SVL actually reaches F.
bool touched_store_content_matches(const Region& region, char width, std::uint64_t group,
                                   std::uint64_t vector_length, std::uint64_t tag,
                                   std::uint64_t touched) {
  const std::uint64_t bytes = element_bytes(width);
  require(touched % bytes == 0 && vector_length % bytes == 0, "store touched-byte element precondition");
  const std::uint64_t mask = bytes == 8 ? ~0ULL : ((1ULL << (8 * bytes)) - 1ULL);
  for (std::uint64_t offset = 0; offset < touched; offset += bytes) {
    const std::uint64_t vector_index = (offset / vector_length) % group;
    const std::uint64_t expected = (tag + vector_index) & mask;
    std::uint64_t observed = 0;
    std::memcpy(&observed, region.data + offset, static_cast<std::size_t>(bytes));
    if ((observed & mask) != expected) return false;
  }
  return true;
}

std::uint64_t ticks_to_ns(std::uint64_t ticks, mach_timebase_info_data_t timebase) {
  return (ticks * static_cast<std::uint64_t>(timebase.numer)) / static_cast<std::uint64_t>(timebase.denom);
}

struct TimedLoad {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t ticks = 0;
  std::uint64_t checksum = 0;
};

TimedLoad timed_load_once(LoadFn load, const Region& region, std::uint64_t group,
                          std::uint64_t vector_length, std::uint64_t expected) {
  validate_leaf_call(region.data, region.footprint, kFrozenIterations, group, vector_length);
  const std::uint64_t begin = mach_absolute_time();
  const std::uint64_t observed = load(region.data, region.footprint, kFrozenIterations);
  const std::uint64_t end = mach_absolute_time();
  require(observed == expected, "all-group/all-lane native load checksum");
  return TimedLoad{begin, end, end - begin, observed};
}

struct TimedStore {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t ticks = 0;
  std::uint64_t signature = 0;
  std::uint64_t touched = 0;
  bool full_capacity_traversed = false;
  bool touched_content_verified = false;
  bool untouched_suffix_samples_checked = false;
  bool untouched_suffix_samples_intact = false;
  bool guards_intact = false;
};

TimedStore timed_store_once(StoreFn store, Region& region, char width, std::uint64_t group,
                            std::uint64_t vector_length) {
  validate_leaf_call(region.data, region.footprint, kFrozenIterations, group, vector_length);
  const std::uint64_t touched = touched_bytes(region.footprint, group, vector_length);
  const SuffixSamples suffix = capture_untouched_suffix_samples(region, touched);
  const std::uint64_t expected = store_checksum_expected(region.footprint, kFrozenIterations, group, kStoreTag);
  const std::uint64_t begin = mach_absolute_time();
  const std::uint64_t observed = store(region.data, region.footprint, kFrozenIterations, kStoreTag);
  const std::uint64_t end = mach_absolute_time();
  require(observed == expected, "native store tag/count signature");
  const bool content = touched_store_content_matches(region, width, group, vector_length, kStoreTag, touched);
  const bool suffix_intact = suffix_samples_intact(region, suffix);
  const bool guards = region.guards_intact();
  require(content, "native touched store content");
  require(suffix_intact, "native untouched suffix samples");
  require(guards, "native store bounds guards");
  return TimedStore{begin, end, end - begin, observed, touched, touched == region.footprint, content,
                    suffix.checked, suffix_intact, guards};
}

std::uint64_t cpu_read_loop(const std::uint8_t* base, std::uint64_t bytes) {
  volatile const std::uint8_t* observed = base;
  std::uint64_t checksum = 0;
  for (std::uint64_t outer = 0; outer < kCpuOuter; ++outer)
    for (std::uint64_t offset = 0; offset < bytes; offset += kCpuStride) checksum += observed[offset];
  std::atomic_signal_fence(std::memory_order_seq_cst);
  return checksum;
}

struct ControlObservation {
  TimedLoad sme{};
  std::uint64_t cpu_begin = 0;
  std::uint64_t cpu_end = 0;
  std::uint64_t cpu_ticks = 0;
  std::uint64_t cpu_checksum = 0;
  std::uint64_t overlap_begin = 0;
  std::uint64_t overlap_end = 0;
  std::uint64_t overlap_ticks = 0;
  bool overlap_required = false;
  bool overlap_qualifies = true;
  bool guards_intact = false;
};

ControlObservation sme_only_once(std::uint64_t vector_length) {
  Region region(kInterferenceFootprint);
  region.fill();
  LoadFn load = load_for('W', 2);
  require(load != nullptr, "native interference leaf dispatch");
  const std::uint64_t expected = load_checksum_expected(region.data, region.footprint, kFrozenIterations, 2, vector_length);
  const TimedLoad timed = timed_load_once(load, region, 2, vector_length, expected);
  require(region.guards_intact(), "native SME-only guards");
  ControlObservation result;
  result.sme = timed;
  result.guards_intact = true;
  return result;
}

ControlObservation read_control_once(bool disjoint, std::uint64_t vector_length) {
  Region sme(kInterferenceFootprint);
  Region other(kInterferenceFootprint);
  sme.fill();
  other.fill();
  LoadFn load = load_for('W', 2);
  require(load != nullptr, "native interference leaf dispatch");
  const std::uint64_t expected = load_checksum_expected(sme.data, sme.footprint, kFrozenIterations, 2, vector_length);
  std::mutex gate_mutex;
  std::condition_variable gate_cv;
  bool go = false;
  bool cpu_started = false;
  std::uint64_t cpu_begin = 0;
  std::uint64_t cpu_end = 0;
  std::uint64_t cpu_checksum = 0;
  const std::uint8_t* cpu_base = disjoint ? other.data : sme.data;
  std::thread cpu([&] {
    {
      std::unique_lock<std::mutex> lock(gate_mutex);
      gate_cv.wait(lock, [&] { return go; });
    }
    cpu_begin = mach_absolute_time();
    {
      std::lock_guard<std::mutex> lock(gate_mutex);
      cpu_started = true;
    }
    gate_cv.notify_one();
    cpu_checksum = cpu_read_loop(cpu_base, kInterferenceFootprint);
    cpu_end = mach_absolute_time();
  });
  {
    std::lock_guard<std::mutex> lock(gate_mutex);
    go = true;
  }
  gate_cv.notify_one();
  {
    std::unique_lock<std::mutex> lock(gate_mutex);
    gate_cv.wait(lock, [&] { return cpu_started; });
  }
  TimedLoad timed{};
  try {
    timed = timed_load_once(load, sme, 2, vector_length, expected);
  } catch (...) {
    cpu.join();
    throw;
  }
  cpu.join();
  const std::uint64_t overlap_begin = std::max(timed.begin, cpu_begin);
  const std::uint64_t overlap_end = std::min(timed.end, cpu_end);
  const bool overlap = overlap_end > overlap_begin;
  ControlObservation result;
  result.sme = timed;
  result.cpu_begin = cpu_begin;
  result.cpu_end = cpu_end;
  result.cpu_ticks = cpu_end - cpu_begin;
  result.cpu_checksum = cpu_checksum;
  result.overlap_begin = overlap ? overlap_begin : 0;
  result.overlap_end = overlap ? overlap_end : 0;
  result.overlap_ticks = overlap ? overlap_end - overlap_begin : 0;
  result.overlap_required = true;
  result.overlap_qualifies = overlap;
  result.guards_intact = sme.guards_intact() && other.guards_intact();
  require(result.guards_intact, "native read-control guards");
  return result;
}

ControlObservation handoff_control_once(std::uint64_t vector_length) {
  Region region(kInterferenceFootprint);
  region.fill();
  LoadFn load = load_for('W', 2);
  require(load != nullptr, "native handoff leaf dispatch");
  std::mutex handoff_mutex;
  std::condition_variable handoff_cv;
  bool written = false;
  std::uint64_t cpu_begin = 0;
  std::uint64_t cpu_end = 0;
  std::thread cpu([&] {
    cpu_begin = mach_absolute_time();
    for (std::uint64_t index = 0; index < region.footprint; ++index) region.data[index] = 0x3c;
    std::atomic_thread_fence(std::memory_order_release);
    cpu_end = mach_absolute_time();
    {
      std::lock_guard<std::mutex> lock(handoff_mutex);
      written = true;
    }
    handoff_cv.notify_one();
  });
  {
    std::unique_lock<std::mutex> lock(handoff_mutex);
    handoff_cv.wait(lock, [&] { return written; });
  }
  TimedLoad timed{};
  try {
    const std::uint64_t expected =
        load_checksum_expected(region.data, region.footprint, kFrozenIterations, 2, vector_length);
    timed = timed_load_once(load, region, 2, vector_length, expected);
  } catch (...) {
    cpu.join();
    throw;
  }
  cpu.join();
  require(region.guards_intact(), "native handoff guards");
  ControlObservation result;
  result.sme = timed;
  result.cpu_begin = cpu_begin;
  result.cpu_end = cpu_end;
  result.cpu_ticks = cpu_end - cpu_begin;
  result.guards_intact = true;
  return result;
}

std::string json_bool(bool value) { return value ? "true" : "false"; }

std::uint64_t footprint_exponent(std::uint64_t footprint) {
  std::uint64_t exponent = 0;
  while (footprint > 1) {
    footprint >>= 1;
    ++exponent;
  }
  return exponent;
}

std::string transfer_cell_id(const Cell& cell) {
  std::ostringstream out;
  out << (cell.kind == "load" ? "LOAD" : "STORE") << '-' << cell.width << "-G" << cell.group
      << "-F2P" << footprint_exponent(cell.footprint);
  return out.str();
}

std::string phase_name(std::uint64_t index) { return index < kWarmups ? "warmup" : "recorded"; }

std::uint64_t phase_ordinal(std::uint64_t index) { return index < kWarmups ? index + 1 : index - kWarmups + 1; }

std::string load_record_json(const TimedLoad& result, const Cell& cell, std::uint64_t vector_length,
                             mach_timebase_info_data_t timebase, std::uint64_t phase_index) {
  const std::uint64_t requested = requested_payload_bytes(cell.group, vector_length);
  const std::uint64_t touched = touched_bytes(cell.footprint, cell.group, vector_length);
  const std::uint64_t traversal = full_traversal_iterations(cell.footprint, cell.group, vector_length);
  std::ostringstream out;
  out << "{\"phase\":\"" << phase_name(phase_index) << "\",\"ordinal\":" << phase_ordinal(phase_index)
      << ",\"kind\":\"load\",\"width\":\"" << cell.width << "\",\"group\":" << cell.group
      << ",\"footprint_bytes\":" << cell.footprint << ",\"iterations\":" << kFrozenIterations
      << ",\"matrix_invocations\":0,\"timed_leaf_calls\":1,\"begin_raw_ticks\":" << result.begin
      << ",\"end_raw_ticks\":" << result.end << ",\"elapsed_raw_ticks\":" << result.ticks
      << ",\"elapsed_ns\":" << ticks_to_ns(result.ticks, timebase)
      << ",\"load_checksum\":" << result.checksum
      << ",\"unique_capacity_bytes\":" << cell.footprint
      << ",\"active_predicate_bytes_per_iteration\":" << cell.group * vector_length
      << ",\"requested_payload_bytes\":" << requested << ",\"touched_bytes\":" << touched
      << ",\"capacity_bytes_not_traversed\":" << cell.footprint - touched
      << ",\"full_traversal_iterations\":" << traversal
      << ",\"full_capacity_traversed\":" << json_bool(touched == cell.footprint)
      << ",\"guards_intact\":true}";
  return out.str();
}

std::string store_record_json(const TimedStore& result, const Cell& cell, std::uint64_t vector_length,
                              mach_timebase_info_data_t timebase, std::uint64_t phase_index) {
  const std::uint64_t requested = requested_payload_bytes(cell.group, vector_length);
  const std::uint64_t traversal = full_traversal_iterations(cell.footprint, cell.group, vector_length);
  std::ostringstream out;
  out << "{\"phase\":\"" << phase_name(phase_index) << "\",\"ordinal\":" << phase_ordinal(phase_index)
      << ",\"kind\":\"store\",\"width\":\"" << cell.width << "\",\"group\":" << cell.group
      << ",\"footprint_bytes\":" << cell.footprint << ",\"iterations\":" << kFrozenIterations
      << ",\"matrix_invocations\":0,\"timed_leaf_calls\":1,\"begin_raw_ticks\":" << result.begin
      << ",\"end_raw_ticks\":" << result.end << ",\"elapsed_raw_ticks\":" << result.ticks
      << ",\"elapsed_ns\":" << ticks_to_ns(result.ticks, timebase)
      << ",\"store_signature\":" << result.signature
      << ",\"unique_capacity_bytes\":" << cell.footprint
      << ",\"active_predicate_bytes_per_iteration\":" << cell.group * vector_length
      << ",\"requested_payload_bytes\":" << requested << ",\"touched_bytes\":" << result.touched
      << ",\"capacity_bytes_not_traversed\":" << cell.footprint - result.touched
      << ",\"full_traversal_iterations\":" << traversal
      << ",\"full_capacity_traversed\":" << json_bool(result.full_capacity_traversed)
      << ",\"touched_content_verified\":" << json_bool(result.touched_content_verified)
      << ",\"untouched_suffix_samples_checked\":" << json_bool(result.untouched_suffix_samples_checked)
      << ",\"untouched_suffix_samples_intact\":" << json_bool(result.untouched_suffix_samples_intact)
      << ",\"guards_intact\":" << json_bool(result.guards_intact) << '}';
  return out.str();
}

std::string run_transfer_session(const Cell& cell, std::uint64_t session, std::uint64_t seed) {
  require((cell.kind == "load" || cell.kind == "store") && valid_group(cell.group) && valid_footprint(cell.footprint),
          "invalid frozen native transfer cell");
  require(session >= 1 && session <= kSessions && seed == kFrozenSeed, "frozen native session/seed");
  const std::uint64_t vector_length = transfer_sme_vl_bytes();
  require(vector_length == calibrated_vector_length_bytes, "native runtime SVL differs from native calibration");
  mach_timebase_info_data_t timebase{};
  require(mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom != 0, "native timebase");
  std::vector<std::string> records;
  records.reserve(static_cast<std::size_t>(kWarmups + kRecorded));
  for (std::uint64_t phase = 0; phase < kWarmups + kRecorded; ++phase) {
    if (cell.kind == "load") {
      Region region(cell.footprint);
      region.fill();
      LoadFn load = load_for(cell.width, cell.group);
      require(load != nullptr, "native load leaf dispatch");
      const std::uint64_t expected = load_checksum_expected(region.data, region.footprint, kFrozenIterations,
                                                           cell.group, vector_length);
      const TimedLoad timed = timed_load_once(load, region, cell.group, vector_length, expected);
      require(region.guards_intact(), "native load bounds guards");
      records.push_back(load_record_json(timed, cell, vector_length, timebase, phase));
    } else {
      Region region(cell.footprint);
      region.fill();
      StoreFn store = store_for(cell.width, cell.group);
      require(store != nullptr, "native store leaf dispatch");
      const TimedStore timed = timed_store_once(store, region, cell.width, cell.group, vector_length);
      records.push_back(store_record_json(timed, cell, vector_length, timebase, phase));
    }
  }
  require(records.size() == kWarmups + kRecorded, "native exact transfer record count");
  std::ostringstream out;
  out << "{\"schema\":\"mp5-transfer-worker.v1\",\"worker_kind\":\"native\""
      << ",\"type\":\"TRANSFER_PRODUCTION_SESSION\",\"status\":\"PASS\""
      << ",\"command\":\"--production-session\",\"kind\":\"" << cell.kind << "\""
      << ",\"cell_id\":\"" << transfer_cell_id(cell) << "\""
      << ",\"width\":\"" << cell.width << "\",\"z_groups\":" << cell.group
      << ",\"footprint_bytes\":" << cell.footprint << ",\"session\":" << session
      << ",\"seed\":" << seed << ",\"iterations_per_leaf\":" << kFrozenIterations
      << ",\"calibrated_vector_length_bytes\":" << calibrated_vector_length_bytes
      << ",\"runtime_vector_length_bytes\":" << vector_length
      << ",\"timebase\":{\"numer\":" << timebase.numer << ",\"denom\":" << timebase.denom << '}'
      << ",\"warmup_leaf_record_count\":" << kWarmups << ",\"recorded_leaf_record_count\":" << kRecorded
      << ",\"total_leaf_record_count\":" << records.size() << ",\"no_hidden_timed_leaf_calls\":true"
      << ",\"matrix_invocations\":0,\"matrix_coupled_calls\":[]"
      << ",\"matrix_coverage_disposition\":\"COVERAGE_UNAVAILABLE_WITHIN_ALLOWED_POPULATION\""
      << ",\"topology_affinity_search\":false,\"records\":[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) out << ',';
    out << records[index];
  }
  out << "]}";
  return out.str();
}

std::string interference_record_json(const ControlObservation& result, const std::string& control,
                                     std::uint64_t vector_length, mach_timebase_info_data_t timebase,
                                     std::uint64_t phase_index) {
  const std::uint64_t requested = requested_payload_bytes(2, vector_length);
  const std::uint64_t touched = touched_bytes(kInterferenceFootprint, 2, vector_length);
  const std::uint64_t traversal = full_traversal_iterations(kInterferenceFootprint, 2, vector_length);
  const bool valid = result.guards_intact && (!result.overlap_required || result.overlap_qualifies);
  std::ostringstream out;
  out << "{\"phase\":\"" << phase_name(phase_index) << "\",\"ordinal\":" << phase_ordinal(phase_index)
      << ",\"timed_leaf_calls\":1,\"control\":\"" << control << "\""
      << ",\"record_status\":\"" << (valid ? "PASS" : "INVALID_OVERLAP") << "\""
      << ",\"sme_width\":\"W\",\"sme_z_groups\":2,\"sme_footprint_bytes\":" << kInterferenceFootprint
      << ",\"iterations_per_leaf\":" << kFrozenIterations << ",\"iterations\":" << kFrozenIterations
      << ",\"matrix_invocations\":0"
      << ",\"sme_active_predicate_bytes_per_iteration\":" << 2 * vector_length
      << ",\"sme_requested_payload_bytes\":" << requested << ",\"sme_touched_bytes\":" << touched
      << ",\"sme_capacity_bytes_not_traversed\":" << kInterferenceFootprint - touched
      << ",\"sme_full_traversal_iterations\":" << traversal
      << ",\"sme_full_capacity_traversed\":" << json_bool(touched == kInterferenceFootprint)
      << ",\"sme_begin_raw_ticks\":" << result.sme.begin << ",\"sme_end_raw_ticks\":" << result.sme.end
      << ",\"sme_elapsed_raw_ticks\":" << result.sme.ticks
      << ",\"sme_elapsed_ns\":" << ticks_to_ns(result.sme.ticks, timebase)
      << ",\"sme_checksum\":" << result.sme.checksum
      << ",\"cpu_begin_raw_ticks\":" << result.cpu_begin << ",\"cpu_end_raw_ticks\":" << result.cpu_end
      << ",\"cpu_elapsed_raw_ticks\":" << result.cpu_ticks
      << ",\"cpu_elapsed_ns\":" << ticks_to_ns(result.cpu_ticks, timebase)
      << ",\"cpu_checksum\":" << result.cpu_checksum
      << ",\"actual_overlap_raw_ticks\":" << result.overlap_ticks
      << ",\"overlap_begin_raw_ticks\":" << result.overlap_begin
      << ",\"overlap_end_raw_ticks\":" << result.overlap_end
      << ",\"overlap_required\":" << json_bool(result.overlap_required)
      << ",\"minimum_qualifying_overlap_rule\":\"strictly_greater_than_zero_raw_ticks\""
      << ",\"overlap_qualifies\":" << json_bool(result.overlap_qualifies)
      << ",\"guards_intact\":" << json_bool(result.guards_intact) << '}';
  return out.str();
}

std::string run_interference_session(const std::string& control, std::uint64_t session, std::uint64_t seed) {
  require(session >= 1 && session <= kSessions && seed == kFrozenSeed, "frozen native interference session/seed");
  const std::uint64_t vector_length = transfer_sme_vl_bytes();
  require(vector_length == calibrated_vector_length_bytes, "native runtime SVL differs from native calibration");
  mach_timebase_info_data_t timebase{};
  require(mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom != 0, "native timebase");
  std::vector<std::string> records;
  records.reserve(static_cast<std::size_t>(kWarmups + kRecorded));
  bool all_valid = true;
  for (std::uint64_t phase = 0; phase < kWarmups + kRecorded; ++phase) {
    ControlObservation result;
    if (control == "SME_ONLY") result = sme_only_once(vector_length);
    else if (control == "CPU_SHARED_READ_ONLY") result = read_control_once(false, vector_length);
    else if (control == "CPU_DISJOINT_READ_ONLY") result = read_control_once(true, vector_length);
    else if (control == "CPU_WRITE_THEN_SME_READ_HANDOFF") result = handoff_control_once(vector_length);
    else fail("unknown frozen native interference control");
    all_valid = all_valid && result.guards_intact && (!result.overlap_required || result.overlap_qualifies);
    records.push_back(interference_record_json(result, control, vector_length, timebase, phase));
  }
  require(records.size() == kWarmups + kRecorded, "native exact interference record count");
  std::ostringstream out;
  out << "{\"schema\":\"mp5-transfer-worker.v1\",\"worker_kind\":\"native\""
      << ",\"type\":\"INTERFERENCE_PRODUCTION_SESSION\",\"status\":\""
      << (all_valid ? "PASS" : "INVALID_OVERLAP") << "\""
      << ",\"command\":\"--interference-session\",\"control\":\"" << control << "\""
      << ",\"session\":" << session << ",\"seed\":" << seed
      << ",\"iterations_per_leaf\":" << kFrozenIterations
      << ",\"calibrated_vector_length_bytes\":" << calibrated_vector_length_bytes
      << ",\"runtime_vector_length_bytes\":" << vector_length
      << ",\"timebase\":{\"numer\":" << timebase.numer << ",\"denom\":" << timebase.denom << '}'
      << ",\"warmup_leaf_record_count\":" << kWarmups << ",\"recorded_leaf_record_count\":" << kRecorded
      << ",\"total_leaf_record_count\":" << records.size() << ",\"no_hidden_timed_leaf_calls\":true"
      << ",\"matrix_invocations\":0,\"matrix_coupled_calls\":[]"
      << ",\"matrix_coverage_disposition\":\"COVERAGE_UNAVAILABLE_WITHIN_ALLOWED_POPULATION\""
      << ",\"topology_affinity_search\":false,\"records\":[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) out << ',';
    out << records[index];
  }
  out << "]}";
  return out.str();
}

Cell parse_transfer_command(int argc, char** argv, std::uint64_t* session, std::uint64_t* seed) {
  require(argc == 18, "native transfer command has an exact 17-argument surface");
  require(std::string(argv[1]) == "--production-session" && std::string(argv[2]) == "--kind" &&
              std::string(argv[4]) == "--width" && std::string(argv[6]) == "--group" &&
              std::string(argv[8]) == "--footprint-bytes" && std::string(argv[10]) == "--session" &&
              std::string(argv[12]) == "--iterations" && std::string(argv[14]) == "--seed",
          "native transfer command flag order");
  Cell cell;
  cell.kind = argv[3];
  require(cell.kind == "load" || cell.kind == "store", "native kind must be load or store");
  cell.width = parse_width(argv[5]);
  cell.group = parse_decimal(argv[7], "group");
  cell.footprint = parse_decimal(argv[9], "footprint-bytes");
  *session = parse_decimal(argv[11], "session");
  const std::uint64_t iterations = parse_decimal(argv[13], "iterations");
  *seed = parse_decimal(argv[15], "seed");
  require(valid_group(cell.group) && valid_footprint(cell.footprint), "native transfer cell is outside frozen grid");
  require(*session >= 1 && *session <= kSessions && iterations == kFrozenIterations && *seed == kFrozenSeed,
          "native transfer session/iterations/seed differs from frozen values");
  return cell;
}

std::string parse_interference_command(int argc, char** argv, std::uint64_t* session, std::uint64_t* seed) {
  require(argc == 12, "native interference command has an exact 11-argument surface");
  require(std::string(argv[1]) == "--interference-session" && std::string(argv[2]) == "--control" &&
              std::string(argv[4]) == "--session" && std::string(argv[6]) == "--iterations" &&
              std::string(argv[8]) == "--seed",
          "native interference command flag order");
  const std::string control(argv[3]);
  require(control == "SME_ONLY" || control == "CPU_SHARED_READ_ONLY" || control == "CPU_DISJOINT_READ_ONLY" ||
              control == "CPU_WRITE_THEN_SME_READ_HANDOFF",
          "native control is outside frozen registry");
  *session = parse_decimal(argv[5], "session");
  const std::uint64_t iterations = parse_decimal(argv[7], "iterations");
  *seed = parse_decimal(argv[9], "seed");
  require(*session >= 1 && *session <= kSessions && iterations == kFrozenIterations && *seed == kFrozenSeed,
          "native interference session/iterations/seed differs from frozen values");
  return control;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    require(argc == 18 || argc == 12, "usage: transfer-worker --production-session ... --vector-length-bytes N, or --interference-session ... --vector-length-bytes N");
    require(std::string(argv[argc - 2]) == "--vector-length-bytes", "calibrated vector length is required");
    calibrated_vector_length_bytes = parse_decimal(argv[argc - 1], "vector-length-bytes");
    require(calibrated_vector_length_bytes >= 16 && calibrated_vector_length_bytes <= 2048 && calibrated_vector_length_bytes % 8 == 0, "invalid calibrated vector length");
    kFrozenIterations = parse_decimal(argv[argc - 5], "iterations");
    require(kFrozenIterations == 1 || kFrozenIterations == 8 || kFrozenIterations == 64 || kFrozenIterations == 1024, "invalid calibrated ladder rung");
    std::uint64_t session = 0;
    std::uint64_t seed = 0;
    const std::string command(argv[1]);
    if (command == "--production-session") {
      const Cell cell = parse_transfer_command(argc, argv, &session, &seed);
      std::cout << run_transfer_session(cell, session, seed) << '\n';
    } else if (command == "--interference-session") {
      const std::string control = parse_interference_command(argc, argv, &session, &seed);
      std::cout << run_interference_session(control, session, seed) << '\n';
    } else {
      fail("native accepts neither native --qualification nor --pilot; only its two exact production commands");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "native transfer worker error: " << error.what() << '\n';
    return 2;
  }
}
