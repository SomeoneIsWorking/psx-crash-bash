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
  if (!open_.empty() && packet != 0u) {
    open_.back().leaves.push_back({call, packet});
  }
}

void PacketCollector::commit(Core &core) {
  for (const Pending &pending : pending_) {
    const Calls &calls = pending.calls;
    if (calls.faces.size() == 1u && calls.leaves.empty()) {
      saveFaceState(core, pending.owner, calls.faces.front());
    } else if (calls.faces.empty()) {
      saveLeafState(core, pending.owner, calls.leaves);
    }
  }
  pending_.clear();
}

} // namespace crashbash::render
