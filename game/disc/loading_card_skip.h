#pragma once

class Core;

namespace crashbash {

// Retires the LOADING card draw `FUN_80018B08` at the two BOOT.BIN handoff present call sites; other
// callers (the menu panels) run the guest body.
void registerLoadingCardSkipOverride(Core &core);

} // namespace crashbash
