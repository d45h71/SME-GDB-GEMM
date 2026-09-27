// native local reconstruction: this program has no GEMM interface, matrix dimensions, or affinity API.
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <limits>
#include <mach/mach_time.h>
#include <mutex>
#include <pthread.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" std::uint64_t transfer_sme_vl_bytes();
#define DECLARE_TRANSFER_LOAD(W, G) extern "C" std::uint64_t transfer_sme_load_##W##_g##G(const std::uint8_t*, std::uint64_t, std::uint64_t);
#define DECLARE_TRANSFER_STORE(W, G) extern "C" std::uint64_t transfer_sme_store_##W##_g##G(std::uint8_t*, std::uint64_t, std::uint64_t, std::uint64_t);
#define DECLARE_TRANSFER_WIDTH(W) DECLARE_TRANSFER_LOAD(W, 1) DECLARE_TRANSFER_LOAD(W, 2) DECLARE_TRANSFER_LOAD(W, 4) DECLARE_TRANSFER_STORE(W, 1) DECLARE_TRANSFER_STORE(W, 2) DECLARE_TRANSFER_STORE(W, 4)
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
constexpr std::uint64_t kQualificationFootprint = 2048;
constexpr std::uint64_t kInterferenceFootprint = 2 * 1024 * 1024;
constexpr std::array<std::uint64_t, 4> kPilotLadder{{1, 8, 64, 1024}};
constexpr std::uint64_t kPilotRecordedRepetitions = 3;
constexpr std::uint64_t kCpuStride = 64;
constexpr std::uint64_t kCpuOuter = 16;
constexpr std::uint64_t kStoreTag = 0x1122334455667788ULL;

using LoadFn = std::uint64_t (*)(const std::uint8_t*, std::uint64_t, std::uint64_t);
using StoreFn = std::uint64_t (*)(std::uint8_t*, std::uint64_t, std::uint64_t, std::uint64_t);

[[noreturn]] void fail(const std::string& text) { throw std::runtime_error(text); }
void require(bool value, const std::string& text) { if (!value) fail(text); }
bool valid_group(std::uint64_t group) { return group == 1 || group == 2 || group == 4; }

std::uint64_t element_bytes(char width) {
  switch (width) { case 'B': return 1; case 'H': return 2; case 'W': return 4; case 'D': return 8; }
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
    require(bytes >= kQualificationFootprint && (bytes & (bytes - 1)) == 0, "invalid footprint");
    void* allocation = nullptr;
    require(posix_memalign(&allocation, kAlignment, static_cast<std::size_t>(bytes + 2 * kGuardBytes)) == 0 && allocation,
            "aligned allocation");
    raw = static_cast<std::uint8_t*>(allocation);
    data = raw + kGuardBytes;
    require(reinterpret_cast<std::uintptr_t>(data) % kAlignment == 0, "alignment");
    std::memset(raw, 0xA5, kGuardBytes);
    std::memset(data + footprint, 0x5A, kGuardBytes);
  }
  Region(const Region&) = delete;
  Region& operator=(const Region&) = delete;
  ~Region() { std::free(raw); }
  void fill() {
    for (std::uint64_t i = 0; i < footprint; ++i) data[i] = static_cast<std::uint8_t>((17 * i + 3) & 0xffU);
  }
  bool guards_intact() const {
    for (std::uint64_t i = 0; i < kGuardBytes; ++i) if (raw[i] != 0xA5 || data[footprint + i] != 0x5A) return false;
    return true;
  }
};

void validate_leaf_call(const std::uint8_t* base, std::uint64_t footprint, std::uint64_t iterations,
                        std::uint64_t group, std::uint64_t vector_length) {
  require(base != nullptr && iterations > 0 && valid_group(group) && vector_length >= 16 && vector_length % 8 == 0,
          "leaf precondition");
  const std::uint64_t stride = group * vector_length;
  require(stride / group == vector_length && footprint >= stride && footprint % stride == 0, "integral wrap precondition");
  const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
  require(address % kAlignment == 0 && address <= std::numeric_limits<std::uintptr_t>::max() - footprint,
          "address/alignment/overflow precondition");
}

std::uint64_t load_checksum_expected(const std::uint8_t* base, std::uint64_t footprint, std::uint64_t iterations,
                                     std::uint64_t group, std::uint64_t vector_length) {
  validate_leaf_call(base, footprint, iterations, group, vector_length);
  const std::uint64_t stride = group * vector_length;
  const std::uint64_t lanes = vector_length / 8;
  std::vector<std::uint64_t> accumulators(static_cast<std::size_t>(group * lanes), 0);
  for (std::uint64_t iteration = 0, cursor = 0; iteration < iterations; ++iteration) {
    for (std::uint64_t g = 0; g < group; ++g) {
      for (std::uint64_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t word = 0;
        std::memcpy(&word, base + cursor + g * vector_length + lane * 8, sizeof(word));
        accumulators[static_cast<std::size_t>(g * lanes + lane)] += word;
      }
    }
    cursor += stride;
    if (cursor == footprint) cursor = 0;
  }
  std::vector<std::uint64_t> group_reductions(static_cast<std::size_t>(group), 0);
  for (std::uint64_t g = 0; g < group; ++g) {
    for (std::uint64_t lane = 0; lane < lanes; ++lane)
      group_reductions[static_cast<std::size_t>(g)] += accumulators[static_cast<std::size_t>(g * lanes + lane)];
  }
  auto ror64 = [](std::uint64_t value, unsigned bits) {
    return (value >> bits) | (value << (64U - bits));
  };
  std::uint64_t reduced = group_reductions[0];
  for (std::uint64_t g = 1; g < group; ++g)
    reduced ^= ror64(group_reductions[static_cast<std::size_t>(g)], static_cast<unsigned>(13 * g));
  return reduced ^ iterations;
}

std::uint64_t store_checksum_expected(std::uint64_t footprint, std::uint64_t iterations,
                                      std::uint64_t group, std::uint64_t tag) {
  return tag ^ iterations ^ footprint ^ group;
}

bool store_content_matches(const std::uint8_t* base, std::uint64_t footprint, char width, std::uint64_t group,
                           std::uint64_t vector_length, std::uint64_t tag) {
  const std::uint64_t bytes = element_bytes(width);
  require(footprint % bytes == 0 && vector_length % bytes == 0, "store element precondition");
  const std::uint64_t mask = bytes == 8 ? ~0ULL : ((1ULL << (8 * bytes)) - 1ULL);
  for (std::uint64_t offset = 0; offset < footprint; offset += bytes) {
    const std::uint64_t vector_index = (offset / vector_length) % group;
    const std::uint64_t expected = (tag + vector_index) & mask;
    std::uint64_t observed = 0;
    std::memcpy(&observed, base + offset, static_cast<std::size_t>(bytes));
    if ((observed & mask) != expected) return false;
  }
  return true;
}

std::uint64_t ticks_to_ns(std::uint64_t ticks, mach_timebase_info_data_t timebase) {
  return (ticks * static_cast<std::uint64_t>(timebase.numer)) / static_cast<std::uint64_t>(timebase.denom);
}

std::uint64_t cpu_read_loop(const std::uint8_t* base, std::uint64_t bytes) {
  volatile const std::uint8_t* observed = base;
  std::uint64_t checksum = 0;
  for (std::uint64_t outer = 0; outer < kCpuOuter; ++outer)
    for (std::uint64_t offset = 0; offset < bytes; offset += kCpuStride) checksum += observed[offset];
  std::atomic_signal_fence(std::memory_order_seq_cst);
  return checksum;
}

struct TimedLoad {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t ticks = 0;
  std::uint64_t checksum = 0;
};
TimedLoad timed_load_with_expected(LoadFn load, const Region& region, std::uint64_t iterations,
                                   std::uint64_t group, std::uint64_t vector_length,
                                   std::uint64_t expected) {
  validate_leaf_call(region.data, region.footprint, iterations, group, vector_length);
  const std::uint64_t begin = mach_absolute_time();
  const std::uint64_t observed = load(region.data, region.footprint, iterations);
  const std::uint64_t end = mach_absolute_time();
  require(observed == expected, "all-group/all-lane load checksum");
  return TimedLoad{begin, end, end - begin, observed};
}
TimedLoad timed_load(LoadFn load, const Region& region, std::uint64_t iterations, std::uint64_t group,
                     std::uint64_t vector_length) {
  const std::uint64_t expected =
      load_checksum_expected(region.data, region.footprint, iterations, group, vector_length);
  return timed_load_with_expected(load, region, iterations, group, vector_length, expected);
}

struct StackTask { std::uint64_t vector_length; bool pass = false; };
void* stack_task_entry(void* opaque) {
  auto* task = static_cast<StackTask*>(opaque);
  alignas(128) std::uint8_t local[kQualificationFootprint];
  volatile std::uint8_t reserved_headroom[64 * 1024];
  for (std::size_t i = 0; i < sizeof(reserved_headroom); ++i) reserved_headroom[i] = static_cast<std::uint8_t>(i);
  for (std::uint64_t i = 0; i < kQualificationFootprint; ++i) local[i] = static_cast<std::uint8_t>(i + 7);
  try {
    validate_leaf_call(local, kQualificationFootprint, 1, 1, task->vector_length);
    task->pass = transfer_sme_load_b_g1(local, kQualificationFootprint, 1) ==
                 load_checksum_expected(local, kQualificationFootprint, 1, 1, task->vector_length);
  } catch (...) { task->pass = false; }
  return nullptr;
}

bool stack_pair_qualification(std::uint64_t vector_length) {
  StackTask task{vector_length, false};
  pthread_attr_t attributes;
  require(pthread_attr_init(&attributes) == 0, "pthread attr init");
  require(pthread_attr_setstacksize(&attributes, 1024 * 1024) == 0, "fixed stack size");
  pthread_t thread{};
  const int created = pthread_create(&thread, &attributes, stack_task_entry, &task);
  pthread_attr_destroy(&attributes);
  require(created == 0 && pthread_join(thread, nullptr) == 0, "stack worker");
  return task.pass;
}

std::string qualification_json() {
  const std::uint64_t vector_length = transfer_sme_vl_bytes();
  require(vector_length > 0 && 4 * vector_length <= kQualificationFootprint, "streaming vector length");
  mach_timebase_info_data_t timebase{};
  require(mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom != 0, "timebase");
  std::ostringstream cases;
  bool first = true;
  std::uint64_t count = 0;
  for (char width : std::array<char, 4>{'B', 'H', 'W', 'D'}) {
    for (std::uint64_t group : std::array<std::uint64_t, 3>{1, 2, 4}) {
      Region region(kQualificationFootprint);
      region.fill();
      LoadFn load = load_for(width, group);
      StoreFn store = store_for(width, group);
      require(load && store, "fixed leaf dispatch");
      const std::uint64_t traversals = region.footprint / (group * vector_length);
      require(traversals > 0, "qualification traversal count");
      std::uint64_t poison_probes = 0;
      for (std::uint64_t z_group = 0; z_group < group; ++z_group) {
        for (std::uint64_t lane : std::array<std::uint64_t, 2>{0, vector_length / 8 - 1}) {
          region.fill();
          const std::uint64_t before_expected =
              load_checksum_expected(region.data, region.footprint, 1, group, vector_length);
          const std::uint64_t before = load(region.data, region.footprint, 1);
          const std::uint64_t offset = z_group * vector_length + lane * 8;
          region.data[offset] ^= 1U;
          const std::uint64_t after_expected =
              load_checksum_expected(region.data, region.footprint, 1, group, vector_length);
          const std::uint64_t after = load(region.data, region.footprint, 1);
          require(before == before_expected && after == after_expected && before != after,
                  "group/lane load observability");
          ++poison_probes;
        }
      }
      require(poison_probes == 2 * group, "group/lane poison probe count");
      region.fill();
      const TimedLoad load_result = timed_load(load, region, traversals + 1, group, vector_length);
      require(region.guards_intact(), "load bounds guards");
      const std::uint64_t store_iterations = traversals + 1;
      const std::uint64_t store_result = store(region.data, region.footprint, store_iterations, kStoreTag);
      require(store_result == store_checksum_expected(region.footprint, store_iterations, group, kStoreTag), "store checksum");
      require(store_content_matches(region.data, region.footprint, width, group, vector_length, kStoreTag) &&
              region.guards_intact(), "store segment content and bounds");
      region.data[0] ^= 1U;
      require(!store_content_matches(region.data, region.footprint, width, group, vector_length, kStoreTag),
              "planted mismatch rejection");
      require(store(region.data, region.footprint, store_iterations, kStoreTag) ==
              store_checksum_expected(region.footprint, store_iterations, group, kStoreTag), "store restoration checksum");
      require(store_content_matches(region.data, region.footprint, width, group, vector_length, kStoreTag) &&
              region.guards_intact(), "store restoration");
      if (!first) cases << ',';
      first = false;
      cases << "{\"width\":\"" << width << "\",\"z_groups\":" << group
            << ",\"footprint_bytes\":2048,\"active_predicate_bytes_per_iteration\":" << group * vector_length
            << ",\"load_checksum\":" << load_result.checksum << ",\"load_observable\":true"
            << ",\"load_group_lane_poison_probes\":" << poison_probes
            << ",\"load_wrap_checked\":true,\"store_wrap_checked\":true"
            << ",\"guards_intact\":true,\"store_content_verified\":true"
            << ",\"planted_content_mismatch_rejected\":true}";
      ++count;
    }
  }
  require(stack_pair_qualification(vector_length), "fixed heap/stack path");
  std::ostringstream out;
  out << "{\"type\":\"QUALIFICATION\",\"status\":\"PASS\",\"matrix_invocations\":0"
      << ",\"vector_length_bytes\":" << vector_length << ",\"timebase\":{\"numer\":" << timebase.numer
      << ",\"denom\":" << timebase.denom << "},\"case_count\":" << count << ",\"cases\":[" << cases.str() << "]"
      << ",\"heap_stack_pair\":{\"footprint_bytes\":2048,\"alignment_bytes\":128"
      << ",\"configured_thread_stack_bytes\":1048576,\"reserved_headroom_bytes\":65536,\"pass\":true}"
      << ",\"claim_limit\":\"Functional qualification only; no transfer performance result.\"}";
  return out.str();
}

struct PilotCell { char width; std::uint64_t group; std::uint64_t footprint; };
struct PilotResult {
  PilotCell cell;
  std::uint64_t iterations;
  std::uint64_t repetition;
  bool warmup;
  TimedLoad load;
};
PilotResult pilot_load(const PilotCell& cell, std::uint64_t iterations, std::uint64_t repetition, bool warmup,
                       std::uint64_t vector_length) {
  Region region(cell.footprint);
  region.fill();
  LoadFn load = load_for(cell.width, cell.group);
  require(load, "pilot leaf dispatch");
  const TimedLoad result = timed_load(load, region, iterations, cell.group, vector_length);
  require(region.guards_intact(), warmup ? "pilot warmup guards" : "pilot load guards");
  return PilotResult{cell, iterations, repetition, warmup, result};
}

struct ControlResult {
  std::string name;
  TimedLoad sme{};
  std::uint64_t cpu_begin = 0;
  std::uint64_t cpu_end = 0;
  std::uint64_t cpu_ticks = 0;
  std::uint64_t cpu_checksum = 0;
  std::uint64_t overlap_begin = 0;
  std::uint64_t overlap_end = 0;
  std::uint64_t overlap_ticks = 0;
  bool overlap_qualifies = false;
  bool guards_intact = false;
};

ControlResult sme_only_control(std::uint64_t iterations, std::uint64_t vector_length) {
  Region region(kInterferenceFootprint); region.fill();
  LoadFn load = load_for('W', 2);
  (void)timed_load(load, region, iterations, 2, vector_length);
  ControlResult result;
  result.name = "SME_ONLY";
  result.sme = timed_load(load, region, iterations, 2, vector_length);
  result.guards_intact = region.guards_intact();
  return result;
}

ControlResult read_control(const std::string& name, bool disjoint, std::uint64_t iterations,
                           std::uint64_t vector_length) {
  Region sme(kInterferenceFootprint), other(kInterferenceFootprint);
  sme.fill(); other.fill();
  LoadFn load = load_for('W', 2);
  (void)timed_load(load, sme, iterations, 2, vector_length);
  const std::uint64_t expected =
      load_checksum_expected(sme.data, sme.footprint, iterations, 2, vector_length);
  std::mutex gate_mutex;
  std::condition_variable gate_cv;
  bool go = false;
  bool cpu_started = false;
  std::uint64_t cpu_begin = 0, cpu_end = 0, cpu_checksum = 0;
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
  const TimedLoad sme_result =
      timed_load_with_expected(load, sme, iterations, 2, vector_length, expected);
  cpu.join();
  const std::uint64_t overlap_begin = std::max(sme_result.begin, cpu_begin);
  const std::uint64_t overlap_end = std::min(sme_result.end, cpu_end);
  const bool overlap = overlap_end > overlap_begin;
  ControlResult result;
  result.name = name;
  result.sme = sme_result;
  result.cpu_begin = cpu_begin;
  result.cpu_end = cpu_end;
  result.cpu_ticks = cpu_end - cpu_begin;
  result.cpu_checksum = cpu_checksum;
  result.overlap_begin = overlap ? overlap_begin : 0;
  result.overlap_end = overlap ? overlap_end : 0;
  result.overlap_ticks = overlap ? overlap_end - overlap_begin : 0;
  result.overlap_qualifies = overlap;
  result.guards_intact = sme.guards_intact() && other.guards_intact();
  return result;
}

ControlResult handoff_control(std::uint64_t iterations, std::uint64_t vector_length) {
  Region region(kInterferenceFootprint); region.fill();
  std::mutex handoff_mutex;
  std::condition_variable handoff_cv;
  bool written = false;
  std::uint64_t cpu_begin = 0, cpu_end = 0;
  std::thread cpu([&] {
    cpu_begin = mach_absolute_time();
    for (std::uint64_t i = 0; i < region.footprint; ++i) region.data[i] = 0x3c;
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
  LoadFn load = load_for('W', 2);
  const TimedLoad sme_result = timed_load(load, region, iterations, 2, vector_length);
  cpu.join();
  require(region.guards_intact(), "handoff guards");
  ControlResult result;
  result.name = "CPU_WRITE_THEN_SME_READ_HANDOFF";
  result.sme = sme_result;
  result.cpu_begin = cpu_begin;
  result.cpu_end = cpu_end;
  result.cpu_ticks = cpu_end - cpu_begin;
  result.guards_intact = true;
  return result;
}

std::string control_json(const ControlResult& result, mach_timebase_info_data_t timebase) {
  std::ostringstream out;
  out << "{\"control\":\"" << result.name << "\",\"sme_elapsed_raw_ticks\":" << result.sme.ticks
      << ",\"sme_elapsed_ns\":" << ticks_to_ns(result.sme.ticks, timebase)
      << ",\"sme_begin_raw_ticks\":" << result.sme.begin << ",\"sme_end_raw_ticks\":" << result.sme.end
      << ",\"cpu_begin_raw_ticks\":" << result.cpu_begin << ",\"cpu_end_raw_ticks\":" << result.cpu_end
      << ",\"sme_checksum\":" << result.sme.checksum << ",\"cpu_elapsed_raw_ticks\":" << result.cpu_ticks
      << ",\"cpu_elapsed_ns\":" << ticks_to_ns(result.cpu_ticks, timebase)
      << ",\"cpu_checksum\":" << result.cpu_checksum << ",\"actual_overlap_raw_ticks\":" << result.overlap_ticks
      << ",\"overlap_begin_raw_ticks\":" << result.overlap_begin
      << ",\"overlap_end_raw_ticks\":" << result.overlap_end
      << ",\"minimum_qualifying_overlap_rule\":\"strictly_greater_than_zero_raw_ticks\""
      << ",\"overlap_qualifies\":" << (result.overlap_qualifies ? "true" : "false")
      << ",\"guards_intact\":" << (result.guards_intact ? "true" : "false") << "}";
  return out.str();
}

std::uint64_t median(std::vector<std::uint64_t> values) {
  std::sort(values.begin(), values.end());
  return values.at(values.size() / 2);
}

std::string pilot_json() {
  const std::uint64_t vector_length = transfer_sme_vl_bytes();
  require(vector_length > 0, "streaming vector length");
  mach_timebase_info_data_t timebase{};
  require(mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom != 0, "timebase");
  const std::array<PilotCell, 3> cells{{{'B', 1, 2048}, {'W', 2, 1024 * 1024}, {'D', 4, 8 * 1024 * 1024}}};
  std::vector<PilotResult> results;
  std::array<std::vector<std::uint64_t>, 4> designated_ticks;
  for (std::size_t cell_index = 0; cell_index < cells.size(); ++cell_index) {
    for (std::size_t rung_index = 0; rung_index < kPilotLadder.size(); ++rung_index) {
      const std::uint64_t iterations = kPilotLadder[rung_index];
      results.push_back(pilot_load(cells[cell_index], iterations, 0, true, vector_length));
      for (std::uint64_t repetition = 1; repetition <= kPilotRecordedRepetitions; ++repetition) {
        results.push_back(pilot_load(cells[cell_index], iterations, repetition, false, vector_length));
        if (cell_index == 1) designated_ticks[rung_index].push_back(results.back().load.ticks);
      }
    }
  }
  const std::uint64_t target_ticks =
      (20000000ULL * timebase.denom + timebase.numer - 1) / timebase.numer;
  std::uint64_t frozen_iterations = kPilotLadder.back();
  std::vector<std::uint64_t> designated_medians;
  for (std::size_t rung_index = 0; rung_index < kPilotLadder.size(); ++rung_index) {
    require(designated_ticks[rung_index].size() == kPilotRecordedRepetitions, "pilot repetition count");
    const std::uint64_t rung_median = median(designated_ticks[rung_index]);
    designated_medians.push_back(rung_median);
    if (rung_median >= target_ticks && frozen_iterations == kPilotLadder.back())
      frozen_iterations = kPilotLadder[rung_index];
  }
  const ControlResult sme_only = sme_only_control(frozen_iterations, vector_length);
  const ControlResult shared = read_control("CPU_SHARED_READ_ONLY", false, frozen_iterations, vector_length);
  const ControlResult disjoint = read_control("CPU_DISJOINT_READ_ONLY", true, frozen_iterations, vector_length);
  const ControlResult handoff = handoff_control(frozen_iterations, vector_length);
  require(sme_only.guards_intact && shared.guards_intact && disjoint.guards_intact && handoff.guards_intact,
          "pilot guards");
  require(shared.overlap_qualifies && disjoint.overlap_qualifies, "fixed qualifying overlap");
  std::ostringstream out;
  out << "{\"type\":\"PILOT\",\"status\":\"PASS\",\"matrix_invocations\":0,\"pilot_only\":true"
      << ",\"production_measurement_executed\":false,\"vector_length_bytes\":" << vector_length
      << ",\"timebase\":{\"numer\":" << timebase.numer << ",\"denom\":" << timebase.denom << "}"
      << ",\"batch_ladder\":[1,8,64,1024],\"pilot_cell_count\":3,\"warmups_per_cell_per_rung\":1"
      << ",\"recorded_repetitions_per_cell_per_rung\":3,\"loads\":[";
  for (std::size_t i = 0; i < results.size(); ++i) {
    if (i) out << ',';
    const PilotResult& result = results[i];
    out << "{\"width\":\"" << result.cell.width << "\",\"z_groups\":" << result.cell.group
        << ",\"footprint_bytes\":" << result.cell.footprint << ",\"iterations\":" << result.iterations
        << ",\"repetition\":" << result.repetition
        << ",\"phase\":\"" << (result.warmup ? "warmup" : "recorded") << "\""
        << ",\"active_predicate_bytes_per_iteration\":" << result.cell.group * vector_length
        << ",\"requested_payload_bytes\":" << result.cell.group * vector_length * result.iterations
        << ",\"elapsed_raw_ticks\":" << result.load.ticks << ",\"elapsed_ns\":"
        << ticks_to_ns(result.load.ticks, timebase) << ",\"checksum\":" << result.load.checksum << "}";
  }
  out << "],\"interference_controls\":[" << control_json(sme_only, timebase) << ',' << control_json(shared, timebase)
      << ',' << control_json(disjoint, timebase) << ',' << control_json(handoff, timebase) << ']'
      << ",\"future_production_freeze\":{\"selection_cell\":{\"width\":\"W\",\"z_groups\":2,\"footprint_bytes\":1048576}"
      << ",\"selection_rule\":\"first ladder rung whose median of three designated-cell records reaches 20ms raw-tick floor; otherwise 1024\""
      << ",\"designated_rung_median_ticks\":[";
  for (std::size_t i = 0; i < designated_medians.size(); ++i) {
    if (i) out << ',';
    out << designated_medians[i];
  }
  out << "],\"target_ticks\":" << target_ticks
      << ",\"fixed_inner_iterations\":" << frozen_iterations << ",\"warmups_per_cell\":2,\"sessions\":3"
      << ",\"recorded_repetitions_per_session\":5,\"production_not_run\":true}"
      << ",\"claim_limit\":\"Pilot sets a future limit only; it is not a production transfer or interference observation.\"}";
  return out.str();
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 2) fail("usage: transfer_worker --qualification|--pilot");
    const std::string command(argv[1]);
    if (command == "--qualification") std::cout << qualification_json() << '\n';
    else if (command == "--pilot") std::cout << pilot_json() << '\n';
    else fail("unsupported native command");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "native transfer worker error: " << error.what() << '\n';
    return 2;
  }
}
