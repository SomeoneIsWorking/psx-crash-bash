#pragma once

class Core;

namespace crashbash {

// Retires the LOADING card: BOOT RenderLoadingScreen sets the loading viewport (its black clear) without drawing the
// card model, and UpdateLevelLoad's fade-in for the card is not requested.
void registerLoadingCardSkipOverride(Core &core);

} // namespace crashbash
