#pragma once

class Core;

namespace crashbash {

// Whether this call of the draw-context select `FUN_80018B08` is the LOADING card's, from one of the two BOOT.BIN
// handoff present call sites; it is retired there, and other callers (the menu panels) run the guest body.
[[nodiscard]] bool isLoadingCardDraw(const Core &core);

} // namespace crashbash
