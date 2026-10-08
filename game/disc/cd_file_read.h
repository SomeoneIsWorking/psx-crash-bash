#pragma once

#include <cstdint>

class Core;

namespace crashbash {

void registerCdFileReadOverride(Core &core);
// Drain the resident load queue's pacing pump (0x8001231C) to quiescence, the same completion
// route the retail swap path (0x8001E610) uses, so a queued chain finishes in one call.
void registerLoadPumpDrainOverride(Core &core);
// Retire image generations touched by one mapped 2048-byte CD sector before copying its bytes.
void retireImagesForCdSectorWrite(Core &core, std::uint32_t destination);

} // namespace crashbash
