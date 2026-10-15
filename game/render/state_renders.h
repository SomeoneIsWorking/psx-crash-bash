// game/render/state_renders.h — the renders installed at this title's producers.
#pragma once

#include "state_producer.h"

class Core;

namespace crashbash::render {

// Model faces, text glyphs, panels and screen quads render from the states their native bodies save.
void registerStateRenders(Core &core, psx::present::StateProducerTable &renders);

} // namespace crashbash::render
