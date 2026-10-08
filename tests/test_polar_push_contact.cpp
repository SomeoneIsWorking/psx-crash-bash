#include "polar_push_contact.h"

#include <cassert>
#include <cstdint>

int main() {
  using crashbash::polar::contactThreshold;
  using crashbash::polar::crossesContactThreshold;
  using crashbash::polar::effectRecipe;
  using crashbash::polar::isWithinContactDisk;

  assert(isWithinContactDisk(0, 0, 0));
  assert(isWithinContactDisk(329, 0, 108899));
  assert(isWithinContactDisk(330, 0, 108899));
  assert(isWithinContactDisk(0, 330, 108899));
  assert(!isWithinContactDisk(331, 0, 108899));
  assert(!isWithinContactDisk(0, 331, 108899));
  assert(!isWithinContactDisk(329, 0, 108900));

  assert(contactThreshold(1) == 384);
  assert(crossesContactThreshold(1, 385, 1));
  assert(!crossesContactThreshold(1, 384, 1));
  assert(!crossesContactThreshold(1, 385, 0));

  const auto &first = effectRecipe(0);
  const auto &second = effectRecipe(1);
  const auto &third = effectRecipe(2);
  assert(first.directionOffset == -0x200 && first.lateralOffset == 0x200 && first.sineMultiplier == 17 &&
         first.sineShift == 11);
  assert(second.verticalOffset == -0x2A && second.sideOffset == 0x80 && second.choreographyOffset == -0x100);
  assert(third.directionOffset == -0x400 && third.verticalOffset == -0x48 && third.sideOffset == -0x80 &&
         third.sineMultiplier == 11 && third.sineShift == 10);
  return 0;
}
