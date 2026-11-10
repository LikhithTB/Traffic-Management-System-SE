#include "sim/intersection_manager.h"

#include <algorithm>

namespace {

// 1..40 characters, not only blanks, no control characters, no '|' (config.ini separator).
bool validName(const std::string& n) {
    if (n.empty() || n.size() > IntersectionManager::kMaxNameLength) return false;
    if (n.find_first_not_of(' ') == std::string::npos) return false;
    return std::none_of(n.begin(), n.end(),
                        [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == '|'; });
}

// One arrival rate per lane, each 1..60 vehicles/min.
bool validRates(const IntersectionDef& d) {
    if (d.arrivalPerMin.size() != static_cast<std::size_t>(d.lanes)) return false;
    return std::all_of(d.arrivalPerMin.begin(), d.arrivalPerMin.end(), [](int r) { return r >= 1 && r <= 60; });
}

}  // namespace

Result<void> IntersectionManager::validate(const IntersectionDef& d) {
    if (!validName(d.name)) return Result<void>::failure(ErrorCode::BadLength);
    if (d.lanes < 1 || d.lanes > 8 || !validRates(d)) return Result<void>::failure(ErrorCode::OutOfRange);
    if (d.greenSeconds < 5 || d.greenSeconds > 120) return Result<void>::failure(ErrorCode::OutOfRange);
    return Result<void>::success();
}

Result<int> IntersectionManager::add(IntersectionDef def) {
    if (items_.size() >= kMaxIntersections) return Result<int>::failure(ErrorCode::CapacityExceeded);
    if (def.id == 0) def.id = nextId_;
    if (def.id < 0 || find(def.id) != nullptr) return Result<int>::failure(ErrorCode::OutOfRange);
    const Result<void> ok = validate(def);
    if (!ok.ok()) return Result<int>::failure(ok.error);

    const int id = def.id;
    items_.push_back(std::move(def));
    nextId_ = std::max(nextId_, id + 1);                  // ids are never reused
    return Result<int>::success(id);
}

Result<void> IntersectionManager::edit(int id, IntersectionDef def) {
    auto it = std::find_if(items_.begin(), items_.end(),
                           [id](const IntersectionDef& d) { return d.id == id; });
    if (it == items_.end()) return Result<void>::failure(ErrorCode::OutOfRange);
    def.id = id;
    const Result<void> ok = validate(def);
    if (!ok.ok()) return ok;
    *it = std::move(def);
    return Result<void>::success();
}

Result<void> IntersectionManager::remove(int id) {
    auto it = std::find_if(items_.begin(), items_.end(),
                           [id](const IntersectionDef& d) { return d.id == id; });
    if (it == items_.end()) return Result<void>::failure(ErrorCode::OutOfRange);
    items_.erase(it);
    return Result<void>::success();
}

const IntersectionDef* IntersectionManager::find(int id) const {
    for (const auto& d : items_) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

void IntersectionManager::clear() {
    items_.clear();
    nextId_ = 1;
}
