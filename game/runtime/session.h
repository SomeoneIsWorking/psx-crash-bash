#pragma once

#include "debug/dev_arena.h"
#include "debug/dev_pause.h"
#include "execution/guest_execution.h"
#include "frame/draw_replay.h"
#include "frame/frame_cut.h"
#include "game_runtime.h"
#include "gameplay/polar_push_contact.h"
#include "render/component_incarnation.h"

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
  // The render components' incarnations, which pair a component's samples across in-betweens.
  render::ComponentIncarnations componentIncarnations;
  FrameCut frameCut;
  DrawReplay drawReplay{componentIncarnations};
};

// The session of the Game `core` belongs to; aborts when it has none.
Session &session(Core &core);

} // namespace crashbash
