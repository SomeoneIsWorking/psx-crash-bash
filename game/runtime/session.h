#pragma once

#include "debug/dev_arena.h"
#include "debug/dev_pause.h"
#include "emit_memory.h"
#include "execution/guest_execution.h"
#include "frame/frame_cut.h"
#include "game_runtime.h"
#include "gameplay/polar_push_contact.h"
#include "render/component_incarnation.h"
#include "render/packet_collector.h"
#include "state_producer.h"

class Core;

namespace crashbash {

// Everything one Core owns outside guest RAM, created with its Game.
class Session final : public psx::game::TitleSessionState {
public:
  explicit Session(Core &core) : execution(core) {}

  runtime::GuestExecution execution;
  // Developer requests the control channel arms and the frame driver applies.
  debug::DevArena devArena;
  debug::DevPause devPause;
  polar::ContactCensus polarContactCensus;
  // The render components' incarnations, which the model producer's object keys name.
  render::ComponentIncarnations componentIncarnations;
  // The packets a producer call links, which its state keeps.
  render::PacketCollector packetCollector;
  FrameCut frameCut;
  psx::present::StateProducerTable stateProducers;
  // The host memory a render builds each leaf in, kept so renders allocate none.
  psx::present::Kept<psx::present::HostMemory> leafMemory;
};

// The session of the Game `core` belongs to; aborts when it has none.
Session &session(Core &core);

} // namespace crashbash
