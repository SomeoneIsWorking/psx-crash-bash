#include "render/packet_collector.h"

#include "core.h"

#include <utility>

namespace crashbash::render {

PacketCollector::Scope::Scope(PacketCollector &collector) : collector_(collector) {
  if (collector_.depth_ == collector_.open_.size()) {
    collector_.open_.emplace_back();
  }
  Calls &calls = collector_.open_[collector_.depth_++];
  calls.faces.clear();
  calls.notes.clear();
}

PacketCollector::Scope::~Scope() {
  --collector_.depth_;
}

void PacketCollector::Scope::save(Core &core) {
  if (collector_.pendingUsed_ == collector_.pending_.size()) {
    collector_.pending_.emplace_back();
  }
  Pending &pending = collector_.pending_[collector_.pendingUsed_++];
  pending.owner = core.emission.current();
  Calls &open = collector_.current();
  std::swap(pending.calls.faces, open.faces);
  std::swap(pending.calls.notes, open.notes);
  open.faces.clear();
  open.notes.clear();
}

FaceCall &PacketCollector::faceCall(bool note) {
  if (!note || depth_ == 0) {
    return unnoted_;
  }
  if (faceCallsUsed_ == faceCalls_.size()) {
    faceCalls_.emplace_back();
  }
  current().faces.push_back(faceCallsUsed_);
  return faceCalls_[faceCallsUsed_++];
}

void PacketCollector::noteLeaf(const LeafCall &call, std::uint32_t packet) {
  if (depth_ == 0 || packet == 0u) {
    return;
  }
  if (!bodies_.empty()) {
    current().notes[bodies_.back()].leaves.push_back({call, packet});
    return;
  }
  current().notes.push_back({std::nullopt, {{call, packet}}});
}

PacketCollector::BodyScope::BodyScope(PacketCollector &collector) : collector_(collector) {
  if (collector_.depth_ != 0) {
    collector_.current().notes.emplace_back();
    collector_.bodies_.push_back(collector_.current().notes.size() - 1u);
    active_ = true;
  }
}

PacketCollector::BodyScope::~BodyScope() {
  if (!active_) {
    return;
  }
  // A body that was not finished leaves no note.
  auto &notes = collector_.current().notes;
  if (!notes[collector_.bodies_.back()].body) {
    notes.erase(notes.begin() + static_cast<std::ptrdiff_t>(collector_.bodies_.back()));
  }
  collector_.bodies_.pop_back();
}

void PacketCollector::BodyScope::finish(ComponentBody body) {
  if (active_) {
    collector_.current().notes[collector_.bodies_.back()].body = std::move(body);
  }
}

void PacketCollector::commit(Core &core) {
  for (std::size_t index = 0; index < pendingUsed_; ++index) {
    const Calls &calls = pending_[index].calls;
    if (calls.faces.size() == 1u && calls.notes.empty()) {
      saveFaceState(core, pending_[index].owner, faceCalls_[calls.faces.front()]);
    } else if (calls.faces.empty()) {
      saveLeafState(core, pending_[index].owner, calls.notes);
    }
  }
  pendingUsed_ = 0;
  // An open scope's face calls stay noted until it saves and commits.
  if (depth_ == 0) {
    faceCallsUsed_ = 0;
  }
}

} // namespace crashbash::render
