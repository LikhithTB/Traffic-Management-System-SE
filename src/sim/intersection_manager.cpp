#include "sim/intersection_manager.h"

#include <algorithm>

Result<void> IntersectionManager::validate(const IntersectionDef& d) {
    if (d.name.empty() || d.name.size() > kMaxNameLength ||
        d.name.find_first_not_of(' ') == std::string::npos) {
        return Result<void>::failure(ErrorCode::BadLength);
    }
    for (char c : d.name) {
        if (static_cast<unsigned char>(c) < 0x20 || c == '|') {      // '|' is the config.ini separator
            return Result<void>::failure(ErrorCode::BadLength);
        }
    }
    if (d.lanes < 1 || d.lanes > 8) return Result<void>::failure(ErrorCode::OutOfRange);
    if (d.arrivalPerMin.size() != static_cast<std::size_t>(d.lanes)) {
        return Result<void>::failure(ErrorCode::OutOfRange);
    }
    for (int rate : d.arrivalPerMin) {
        if (rate < 1 || rate > 60) return Result<void>::failure(ErrorCode::OutOfRange);
    }
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
