// game/render/packet_collector.h — what a producer's native bodies drew, until the guest draws the frame.
//
// A producer call (a model, a text, a panel or a quad component) opens a `Scope`; the native bodies it runs
// note their calls here: a mesh body its inputs and the faces it linked, a 2D leaf its arguments and packet.
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

  // No-ops with no scope open: a body called outside a producer has no object to save a state for.
  void noteFaces(FaceCall call);
  void noteLeaf(const LeafCall &call, std::uint32_t packet);

  // Saves each kept scope's state. An object that drew nothing the table still holds, or drew faces and
  // leaves, or more than one mesh, saves nothing and stays as the guest drew it.
  void commit(Core &core);

private:
  struct Calls {
    std::vector<FaceCall> faces;
    std::vector<LeafNote> leaves;
  };
  struct Pending {
    psx::present::RecordKey owner;
    Calls calls;
  };

  std::vector<Calls> open_;
  std::vector<Pending> pending_;
};

} // namespace crashbash::render
