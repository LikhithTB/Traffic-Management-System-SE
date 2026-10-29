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

void EmergencyHandler::clear() {
    queue_ = decltype(queue_)();
}
