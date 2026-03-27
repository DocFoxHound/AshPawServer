#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace ashpaw::simulation {

class SimulationClock {
  public:
    explicit SimulationClock(std::uint16_t tick_rate);

    void add_elapsed(std::chrono::steady_clock::duration elapsed);
    [[nodiscard]] bool should_step() const noexcept;
    void consume_step();
    [[nodiscard]] std::chrono::steady_clock::duration step_duration() const noexcept;
    [[nodiscard]] float step_seconds() const noexcept;
    [[nodiscard]] std::size_t pending_steps() const noexcept;

  private:
    std::chrono::steady_clock::duration accumulator_ {};
    std::chrono::steady_clock::duration step_duration_ {};
    std::uint16_t tick_rate_ {20};
};

}  // namespace ashpaw::simulation
