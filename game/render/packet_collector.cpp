#include "packet_collector.h"

#include "core.h"

#include <utility>

namespace crashbash::render {

PacketCollector::Scope::Scope(PacketCollector &collector) : collector_(collector) {
  collector_.open_.emplace_back();
}

PacketCollector::Scope::~Scope() {
  collector_.open_.pop_back();
}

void PacketCollector::Scope::save(Core &core) {
  collector_.pending_.push_back({core.emission.current(), std::move(collector_.open_.back())});
  collector_.open_.back() = {};
}

void PacketCollector::noteFaces(FaceCall call) {
  if (!open_.empty()) {
    open_.back().faces.push_back(std::move(call));
  }
}

void PacketCollector::noteLeaf(const LeafCall &call, std::uint32_t packet) {
  if (open_.empty() || packet == 0u) {
    return;
  }
  if (!bodies_.empty()) {
    open_.back().notes[bodies_.back()].leaves.push_back({call, packet});
    return;
  }
  open_.back().notes.push_back({std::nullopt, {{call, packet}}});
}

PacketCollector::BodyScope::BodyScope(PacketCollector &collector) : collector_(collector) {
  if (!collector_.open_.empty()) {
    collector_.open_.back().notes.emplace_back();
    collector_.bodies_.push_back(collector_.open_.back().notes.size() - 1u);
    active_ = true;
  }
}

PacketCollector::BodyScope::~BodyScope() {
  if (!active_) {
    return;
  }
  // A body that was not finished leaves no note.
  auto &notes = collector_.open_.back().notes;
  if (!notes[collector_.bodies_.back()].body) {
    notes.erase(notes.begin() + static_cast<std::ptrdiff_t>(collector_.bodies_.back()));
  }
  collector_.bodies_.pop_back();
}

void PacketCollector::BodyScope::finish(ComponentBody body) {
  if (active_) {
    collector_.open_.back().notes[collector_.bodies_.back()].body = std::move(body);
  }
}

void PacketCollector::commit(Core &core) {
  for (const Pending &pending : pending_) {
    const Calls &calls = pending.calls;
    if (calls.faces.size() == 1u && calls.notes.empty()) {
      saveFaceState(core, pending.owner, calls.faces.front());
    } else if (calls.faces.empty()) {
      saveLeafState(core, pending.owner, calls.notes);
    }
  }
  pending_.clear();
}

} // namespace crashbash::render
