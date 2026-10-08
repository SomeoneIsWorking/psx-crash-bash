#pragma once

class Core;

namespace crashbash {

// Execute the finite startup prefix of retail game main 0x8002718C and application main 0x80010158. Their
// lifetime process-loop frames stay on the guest stack; CrashBashFrameDriver owns the repetition.
void runBootPrefix(Core &core);

} // namespace crashbash
