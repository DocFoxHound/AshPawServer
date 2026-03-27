#include "ashpaw/simulation/simulation_clock.hpp"

namespace ashpaw::simulation {

SimulationClock::SimulationClock(std::uint16_t tick_rate)
    : step_duration_(std::chrono::milliseconds(1000 / tick_rate)),
      tick_rate_(tick_rate) {}

void SimulationClock::add_elapsed(std::chrono::steady_clock::duration elapsed) {
    accumulator_ += elapsed;
}

bool SimulationClock::should_step() const noexcept {
    return accumulator_ >= step_duration_;
}

void SimulationClock::consume_step() {
    accumulator_ -= step_duration_;
}

std::chrono::steady_clock::duration SimulationClock::step_duration() const noexcept {
    return step_duration_;
}

float SimulationClock::step_seconds() const noexcept {
    return 1.0F / static_cast<float>(tick_rate_);
}

std::size_t SimulationClock::pending_steps() const noexcept {
    return static_cast<std::size_t>(accumulator_ / step_duration_);
}

}  // namespace ashpaw::simulation
