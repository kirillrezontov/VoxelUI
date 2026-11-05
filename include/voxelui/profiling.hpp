#pragma once
#include <chrono>
#include <cstdint>

namespace voxelui { inline namespace v1 { namespace profiling {
enum class Stage { bindings, rebuild, style, layout, animation, events, drawList };
// Optional UI-thread observation. No clock reads occur without a sink. Durations
// are inclusive; drawList can flush pending update work and events can recurse.
struct Sink {
    void (*record)(Stage, std::uint64_t nanoseconds, void*){};
    void* user{};
};
inline thread_local Sink sink;
class Scope {
    Sink sink_;
    Stage stage_;
    std::chrono::steady_clock::time_point begin_;
public:
    explicit Scope(Stage stage) : sink_(sink), stage_(stage) {
        if(sink_.record) begin_=std::chrono::steady_clock::now();
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope() {
        if(sink_.record) sink_.record(stage_, static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now()-begin_).count()),sink_.user);
    }
};
// Restores a caller's observer, including nested/reentrant host operations.
class Observe {
    Sink previous_{sink};
public:
    explicit Observe(Sink value) { sink=value; }
    Observe(const Observe&)=delete;
    Observe& operator=(const Observe&)=delete;
    ~Observe() { sink=previous_; }
};
} } }
