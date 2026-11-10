#include "sim/emergency_handler.h"

void EmergencyHandler::report(Tick arrival, std::uint64_t vehicleId, int intersectionId, int lane) {
    EmergencyEvent e;
    e.arrivalTick = arrival;
    e.seq = nextSeq_++;
    e.vehicleId = vehicleId;
    e.intersectionId = intersectionId;
    e.lane = lane;
    queue_.push(e);
}

void EmergencyHandler::complete() {
    if (!queue_.empty()) queue_.pop();
}

// A non-active emergency vehicle left a normal green lane before its turn. Drop its event,
// otherwise it would later be served with no vehicle left to trigger the release (FR-04).
void EmergencyHandler::cancel(std::uint64_t vehicleId) {
    std::vector<EmergencyEvent> keep;
    while (!queue_.empty()) {
        if (queue_.top().vehicleId != vehicleId) keep.push_back(queue_.top());
        queue_.pop();
    }
    for (const auto& e : keep) queue_.push(e);
}

void EmergencyHandler::clear() {
    queue_ = decltype(queue_)();
}
