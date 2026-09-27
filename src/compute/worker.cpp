// native worker: one bounded Dispatch reconstruction, no scheduler or service.
// The C++ caller is ordinary nonstreaming code. Every SME leaf owns smstart
// and smstop. Dispatch coordination is private to a single worker command.
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <mach/mach_time.h>
#include <string>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

extern "C" void compute_i8_t1(uint64_t, void*);
extern "C" void compute_i8_t2(uint64_t, void*);
extern "C" void compute_i8_t3(uint64_t, void*);
extern "C" void compute_i8_t4(uint64_t, void*);
extern "C" void compute_i16_t1(uint64_t, void*);
extern "C" void compute_i16_t2(uint64_t, void*);
extern "C" void compute_i16_t3(uint64_t, void*);
extern "C" void compute_i16_t4(uint64_t, void*);
extern "C" void compute_fp16_t1(uint64_t, void*);
extern "C" void compute_fp16_t2(uint64_t, void*);
extern "C" void compute_fp16_t3(uint64_t, void*);
extern "C" void compute_fp16_t4(uint64_t, void*);
extern "C" void compute_bf16_t1(uint64_t, void*);
extern "C" void compute_bf16_t2(uint64_t, void*);
extern "C" void compute_bf16_t3(uint64_t, void*);
extern "C" void compute_bf16_t4(uint64_t, void*);
extern "C" void compute_fp32_t1(uint64_t, void*);
extern "C" void compute_fp32_t2(uint64_t, void*);
extern "C" void compute_fp32_t3(uint64_t, void*);
extern "C" void compute_fp32_t4(uint64_t, void*);
extern "C" void compute_fp64_t1(uint64_t, void*);
extern "C" void compute_fp64_t2(uint64_t, void*);
extern "C" void compute_fp64_t3(uint64_t, void*);
extern "C" void compute_fp64_t4(uint64_t, void*);
extern "C" void compute_fp64_t5(uint64_t, void*);
extern "C" void compute_fp64_t6(uint64_t, void*);
extern "C" void compute_fp64_t7(uint64_t, void*);
extern "C" void compute_fp64_t8(uint64_t, void*);
extern "C" uint64_t compute_sme_svl_bytes(void);

using Kernel = void (*)(uint64_t, void*);
struct Spec {
    const char* wire_format;
    const char* output_kind;
    uint32_t max_tiles;
    uint32_t output_width;
    uint32_t terms_per_accumulator;
    Kernel kernels[8];
};

static const Spec kSpecs[] = {
    {"INT8-to-INT32", "INT32", 4, 4, 4, {compute_i8_t1, compute_i8_t2, compute_i8_t3, compute_i8_t4, nullptr, nullptr, nullptr, nullptr}},
    {"INT16-to-INT32", "INT32", 4, 4, 2, {compute_i16_t1, compute_i16_t2, compute_i16_t3, compute_i16_t4, nullptr, nullptr, nullptr, nullptr}},
    {"FP16-to-FP32", "FP32", 4, 4, 2, {compute_fp16_t1, compute_fp16_t2, compute_fp16_t3, compute_fp16_t4, nullptr, nullptr, nullptr, nullptr}},
    {"BF16-to-FP32", "FP32", 4, 4, 2, {compute_bf16_t1, compute_bf16_t2, compute_bf16_t3, compute_bf16_t4, nullptr, nullptr, nullptr, nullptr}},
    {"FP32-to-FP32", "FP32", 4, 4, 1, {compute_fp32_t1, compute_fp32_t2, compute_fp32_t3, compute_fp32_t4, nullptr, nullptr, nullptr, nullptr}},
    {"FP64-to-FP64", "FP64", 8, 8, 1, {compute_fp64_t1, compute_fp64_t2, compute_fp64_t3, compute_fp64_t4, compute_fp64_t5, compute_fp64_t6, compute_fp64_t7, compute_fp64_t8}}
};

static void emit_error(const char* code) {
    std::cout << "{\"type\":\"ERROR\",\"code\":\"" << code << "\"}" << std::endl;
}

static bool simple_token(const std::string& value) {
    if (value.empty() || value.size() > 64) return false;
    for (unsigned char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

static bool split_exact(const std::string& line, std::vector<std::string>* words) {
    words->clear();
    if (line.empty() || line.size() > 256 || line.front() == ' ' || line.back() == ' ') return false;
    size_t start = 0;
    while (start < line.size()) {
        size_t end = line.find(' ', start);
        if (end == std::string::npos) end = line.size();
        if (end == start || words->size() == 8) return false;
        words->push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return true;
}

static bool positive_u64(const std::string& text, uint64_t upper, uint64_t* value) {
    if (text.empty() || text.size() > 20) return false;
    uint64_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed == 0 || parsed > upper) return false;
    *value = parsed;
    return true;
}

static const Spec* find_spec(const std::string& wire_format) {
    for (const Spec& spec : kSpecs) if (wire_format == spec.wire_format) return &spec;
    return nullptr;
}

static bool all_expected(const uint8_t* data, uint64_t values, const Spec& spec, uint64_t rounds) {
    const uint64_t expected_units = rounds * spec.terms_per_accumulator;
    if (expected_units == 0 || expected_units > 4096) return false;
    if (spec.output_width == 4) {
        uint32_t expected = 0;
        if (std::string(spec.output_kind) == "INT32") {
            expected = static_cast<uint32_t>(expected_units);
        } else {
            const float number = static_cast<float>(expected_units);
            std::memcpy(&expected, &number, sizeof(expected));
        }
        for (uint64_t index = 0; index < values; ++index) {
            uint32_t actual = 0;
            std::memcpy(&actual, data + index * 4, sizeof(actual));
            if (actual != expected) return false;
        }
        return true;
    }
    const double number = static_cast<double>(expected_units);
    uint64_t expected = 0;
    std::memcpy(&expected, &number, sizeof(expected));
    for (uint64_t index = 0; index < values; ++index) {
        uint64_t actual = 0;
        std::memcpy(&actual, data + index * 8, sizeof(actual));
        if (actual != expected) return false;
    }
    return true;
}

static bool write_all(int fd, const uint8_t* data, size_t bytes) {
    size_t offset = 0;
    while (offset < bytes) {
        const ssize_t written = write(fd, data + offset, bytes - offset);
        if (written <= 0) return false;
        offset += static_cast<size_t>(written);
    }
    return fsync(fd) == 0;
}

static bool write_output(const std::string& directory, const std::string& token,
                         const uint8_t* data, size_t bytes, std::string* name) {
    if (!simple_token(token)) return false;
    *name = "output-" + token + ".bin";
    const std::string path = directory + "/" + *name;
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return false;
    const bool good = write_all(fd, data, bytes);
    const int close_status = close(fd);
    return good && close_status == 0;
}

struct ApplyContext {
    Kernel kernel;
    uint8_t* output;
    uint64_t output_per_worker;
    uint64_t rounds;
    std::atomic<uint64_t>* completed;
};

static void apply_kernel(void* opaque, size_t worker_index) {
    auto* context = static_cast<ApplyContext*>(opaque);
    uint8_t* worker_output = context->output + worker_index * context->output_per_worker;
    context->kernel(context->rounds, worker_output);
    context->completed->fetch_add(1, std::memory_order_relaxed);
}

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "--output-dir") return 64;
    const std::string output_dir(argv[2]);
    struct stat directory_stat {};
    if (output_dir.empty() || output_dir.size() > 2048 ||
        stat(output_dir.c_str(), &directory_stat) != 0 || !S_ISDIR(directory_stat.st_mode)) return 65;

    bool begun = false;
    uint64_t svl_bytes = 0;
    mach_timebase_info_data_t timebase {};
    std::string line;
    std::vector<std::string> words;
    while (std::getline(std::cin, line)) {
        if (!split_exact(line, &words)) { emit_error("MALFORMED_COMMAND"); return 66; }
        if (words.size() == 1 && words[0] == "BEGIN") {
            if (begun) { emit_error("DUPLICATE_BEGIN"); return 67; }
            svl_bytes = compute_sme_svl_bytes();
            if (svl_bytes < 16 || svl_bytes > 2048 || (svl_bytes % 8) != 0 ||
                mach_timebase_info(&timebase) != KERN_SUCCESS ||
                timebase.numer == 0 || timebase.denom == 0) {
                emit_error("UNUSABLE_SME_OR_CLOCK");
                return 68;
            }
            begun = true;
            std::cout << "{\"type\":\"READY\",\"worker_protocol\":\"SME-COMPUTE-2\","
                      << "\"svl_bytes\":" << svl_bytes
                      << ",\"timebase_numer\":" << timebase.numer
                      << ",\"timebase_denom\":" << timebase.denom
                      << ",\"coordination\":\"DISPATCH_APPLY_F_RECONSTRUCTION\","
                      << "\"ordinary_caller\":true}" << std::endl;
            continue;
        }
        if (words.size() == 1 && words[0] == "STOP") {
            if (!begun) { emit_error("STOP_BEFORE_BEGIN"); return 69; }
            std::cout << "{\"type\":\"STOPPED\",\"worker_protocol\":\"SME-COMPUTE-2\"}" << std::endl;
            return 0;
        }
        if (!begun || words.size() != 7 || words[0] != "RUN" ||
            !simple_token(words[1]) || (words[6] != "QUALIFY" && words[6] != "CALIBRATE" && words[6] != "MEASURE")) {
            emit_error("INVALID_RUN");
            return 70;
        }
        const Spec* spec = find_spec(words[2]);
        uint64_t tiles = 0;
        uint64_t threads = 0;
        uint64_t rounds = 0;
        if (spec == nullptr || !positive_u64(words[3], spec ? spec->max_tiles : 0, &tiles) ||
            !positive_u64(words[4], 14, &threads) ||
            !positive_u64(words[5], 1024, &rounds)) {
            emit_error("UNSUPPORTED_RUN_PARAMETERS");
            return 71;
        }
        const uint64_t output_per_worker = tiles * svl_bytes;
        const uint64_t total_bytes = output_per_worker * threads;
        const uint64_t total_leaf_invocations = threads;
        const uint64_t total_mopa_loop_trips = threads * rounds;
        if (output_per_worker == 0 || output_per_worker > 8 * 2048 ||
            total_bytes > 14 * 8 * 2048 || total_mopa_loop_trips > 14 * 1024 ||
            total_bytes > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            emit_error("BOUND_EXCEEDED");
            return 72;
        }
        std::vector<uint64_t> words_buffer((static_cast<size_t>(total_bytes) + 7) / 8, 0);
        uint8_t* output = reinterpret_cast<uint8_t*>(words_buffer.data());
        std::atomic<uint64_t> completed{0};
        ApplyContext context{spec->kernels[tiles - 1], output, output_per_worker, rounds, &completed};
        const uint64_t start_ticks = mach_absolute_time();
        dispatch_apply_f(static_cast<size_t>(threads),
                         dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                         &context, apply_kernel);
        const uint64_t end_ticks = mach_absolute_time();
        const uint64_t lanes_per_tile = svl_bytes / spec->output_width;
        const uint64_t output_values = threads * tiles * lanes_per_tile;
        if (end_ticks < start_ticks || completed.load(std::memory_order_relaxed) != total_leaf_invocations ||
            !all_expected(output, output_values, *spec, rounds)) {
            emit_error("OUTPUT_OR_COMPLETION_REJECTED");
            return 73;
        }
        std::string output_file;
        if (!write_output(output_dir, words[1], output, static_cast<size_t>(total_bytes), &output_file)) {
            emit_error("OUTPUT_STORAGE_FAILURE");
            return 74;
        }
        std::cout << "{\"type\":\"RESULT\",\"token\":\"" << words[1]
                  << "\",\"format\":\"" << spec->wire_format
                  << "\",\"output_kind\":\"" << spec->output_kind
                  << "\",\"tiles\":" << tiles
                  << ",\"threads\":" << threads
                  << ",\"mopa_rounds_per_leaf\":" << rounds
                  << ",\"leaf_invocations\":" << total_leaf_invocations
                  << ",\"completed_leaf_invocations\":" << completed.load(std::memory_order_relaxed)
                  << ",\"total_mopa_loop_trips\":" << total_mopa_loop_trips
                  << ",\"terms_per_accumulator\":" << spec->terms_per_accumulator
                  << ",\"expected_accumulator_units\":" << (rounds * spec->terms_per_accumulator)
                  << ",\"svl_bytes\":" << svl_bytes
                  << ",\"lanes_per_tile\":" << lanes_per_tile
                  << ",\"output_file\":\"" << output_file
                  << "\",\"output_bytes\":" << total_bytes
                  << ",\"outputs_equal_expected_units\":true"
                  << ",\"phase\":\"" << words[6]
                  << "\",\"start_ticks\":" << start_ticks
                  << ",\"end_ticks\":" << end_ticks
                  << ",\"timebase_numer\":" << timebase.numer
                  << ",\"timebase_denom\":" << timebase.denom
                  << "}" << std::endl;
    }
    return 75;
}
