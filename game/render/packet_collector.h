// game/render/packet_collector.h — what a producer's native bodies drew, until the guest draws the frame.
//
// A producer call (a model, a text, a panel or a quad component) opens a `Scope`; the native bodies it runs
// note their calls here: a mesh body its inputs and the faces it linked, a 2D leaf its arguments and packet, a
// component body its inputs and the leaves it called.
// When the scope saves, the calls wait for the DrawOTag. `commit` then keeps those whose packets the guest
// is about to draw: a packet another draw relinked or overwrote after its scope ended is not in the table,
// and its call is not saved.
#pragma once

#include "frame_record.h"
#include "leaf_state.h"
#include "model_state.h"

#include <vector>

class Core;

namespace crashbash::render {

class PacketCollector {
public:
  class Scope {
  public:
    explicit Scope(PacketCollector &collector);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
    Scope(Scope &&) = delete;
    Scope &operator=(Scope &&) = delete;

    // Keeps the calls noted so far for `commit`, under the object scope open now; call it once.
    void save(Core &core);

  private:
    PacketCollector &collector_;
  };

  // A component body being drawn: the leaves its calls draw are the body's, not calls of the object. Opens a place
  // for the body's note, so notes the body's own nested calls make come after it. Inert with no scope open.
  class BodyScope {
  public:
    explicit BodyScope(PacketCollector &collector);
    ~BodyScope();
    BodyScope(const BodyScope &) = delete;
    BodyScope &operator=(const BodyScope &) = delete;
    BodyScope(BodyScope &&) = delete;
    BodyScope &operator=(BodyScope &&) = delete;

    // The body is drawn: its note is the body and the leaves it called.
    void finish(ComponentBody body);

  private:
    PacketCollector &collector_;
    bool active_ = false;
  };

  // No-ops with no scope open: a body called outside a producer has no object to save a state for.
  void noteFaces(FaceCall call);
  void noteLeaf(const LeafCall &call, std::uint32_t packet);

  // Saves each kept scope's state. An object that drew nothing the table still holds, or drew faces and
  // leaves, or more than one mesh, saves nothing and stays as the guest drew it.
  void commit(Core &core);

private:
  struct Calls {
    std::vector<FaceCall> faces;
    std::vector<UiNote> notes;
  };
  struct Pending {
    psx::present::RecordKey owner;
    Calls calls;
  };

  // The note of each body being drawn, innermost last: its index in `open_.back().notes`.
  std::vector<std::size_t> bodies_;
  std::vector<Calls> open_;
  std::vector<Pending> pending_;
};

} // namespace crashbash::render
