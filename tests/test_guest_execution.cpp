#include "guest_execution.h"

#include "game.h"
#include "game_runtime.h"
#include "image_identity_state.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"
#include "state_blob.h"
#include "testutil.h"

#include <memory>
#include <optional>
#include <stdexcept>

namespace {

using crashbash::runtime::GuestExecution;
using crashbash::runtime::GuestImage;
constexpr std::uint32_t kEntry = 0x80010000u;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr GuestAddressRange kRange{0x10000u, 0x10200u};

class Runtime final : public GameRuntime {
public:
  void *createContext(Core &core) override {
    return new GuestExecution(core);
  }
  void destroyContext(void *context) override {
    delete static_cast<GuestExecution *>(context);
  }
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}
  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::direct();
  }
  bool guestVramIsPicture(const Game &) const override {
    return false;
  }
};

std::unique_ptr<Game> makeGame(Runtime &runtime) {
  psxport_install_game(runtime);
  return std::make_unique<Game>();
}

GuestExecution &context(Core &core) {
  return *static_cast<GuestExecution *>(core.gameCtx);
}

void nativeValue(Core *core) {
  core->r[2] = 42u;
}

void nativeOriginal(Core *core) {
  crashbash::runtime::callOriginal(*core, GuestImage::Menu, kEntry);
  core->r[2] += 10u;
}

void test_pending_registration_reload_and_wrong_image_refusal() {
  Runtime runtime;
  auto game = makeGame(runtime);
  Core &core = game->core;
  GuestExecution &execution = context(core);
  crashbash::runtime::registerNativeOverride(core, GuestImage::Menu, kEntry, "menu-value", nativeValue);
  CHECK(!execution.activeKey(GuestImage::Menu, kEntry));
  const auto first = core.imageCatalog().activate("menu", kRange, 1u);
  execution.bindAuthenticatedImage(GuestImage::Menu, first, kRange);
  CHECK(core.nativeDispatcher().isInstalled({first, kEntry}));
  core.r[31] = kReturn;
  crashbash::runtime::dispatchGuest(core, kEntry);
  CHECK_EQ(core.r[2], 42u);
  const auto replacement = core.imageCatalog().activate("polar", kRange, 2u);
  CHECK(!execution.activeKey(GuestImage::Menu, kEntry));
  CHECK_EQ(execution.original(GuestImage::Menu, kEntry, psx::cpu::ExecutionBudget::fromCycles(100)).reason,
           psx::cpu::ExecutionExitReason::Fault);
  execution.bindAuthenticatedImage(GuestImage::Dat22510, replacement, kRange);
  CHECK(!execution.activeKey(GuestImage::Menu, kEntry));
  CHECK(!core.nativeDispatcher().isInstalled({replacement, kEntry}));
  const auto reload = core.imageCatalog().activate("menu", kRange, 1u);
  execution.bindAuthenticatedImage(GuestImage::Menu, reload, kRange);
  CHECK(!core.nativeDispatcher().isInstalled({first, kEntry}));
  CHECK(core.nativeDispatcher().isInstalled({reload, kEntry}));
  execution.unbindImage(GuestImage::Menu);
  CHECK(!core.nativeDispatcher().isInstalled({reload, kEntry}));
  CHECK(!execution.activeKey(GuestImage::Menu, kEntry));
}

void test_original_uses_dynarec_and_restores_interception() {
  Runtime runtime;
  auto game = makeGame(runtime);
  Core &core = game->core;
  GuestExecution &execution = context(core);
  const auto image = core.imageCatalog().activate("menu", kRange, 1u);
  core.mem_w32(kEntry, 0x24020007u);      // addiu v0, zero, 7
  core.mem_w32(kEntry + 4u, 0x03e00008u); // jr ra
  core.mem_w32(kEntry + 8u, 0u);          // delay-slot nop
  execution.bindAuthenticatedImage(GuestImage::Menu, image, kRange);
  crashbash::runtime::registerNativeOverride(core, GuestImage::Menu, kEntry, "menu-original", nativeOriginal);
  core.r[31] = kReturn;
  core.r[29] = 0x801ff000u;
  crashbash::runtime::dispatchGuest(core, kEntry);
  CHECK_EQ(core.r[2], 17u);
  CHECK_EQ(core.r[29], 0x801ff000u);
  CHECK_EQ(core.r[31], kReturn);
  CHECK(core.nativeDispatcher().intercepts({image, kEntry}));
  CHECK(core.lightrecExecutor().counters().executedBlocks > 0u);
  CHECK(core.lightrecExecutor().counters().executedInstructions > 0u);
  CHECK_EQ(core.lightrecExecutor().counters().fallback.calls, 0u);
  core.mem_w32(kEntry, 0x24020009u); // same generation, changed code must invalidate the compiled block
  crashbash::runtime::dispatchGuest(core, kEntry);
  CHECK_EQ(core.r[2], 19u);
  CHECK_EQ(execution.original(GuestImage::Boot, kEntry, psx::cpu::ExecutionBudget::fromCycles(100)).reason,
           psx::cpu::ExecutionExitReason::Fault);
}

void test_original_budget_exit_exposes_live_loop_state() {
  Runtime runtime;
  auto game = makeGame(runtime);
  Core &core = game->core;
  GuestExecution &execution = context(core);
  const auto image = core.imageCatalog().activate("menu", kRange, 1u);
  core.mem_w32(kEntry, 0x2508ffffu);       // addiu t0, t0, -1
  core.mem_w32(kEntry + 4u, 0x1500fffeu);  // bne t0, zero, kEntry
  core.mem_w32(kEntry + 8u, 0u);           // delay-slot nop
  core.mem_w32(kEntry + 12u, 0x03e00008u); // jr ra
  core.mem_w32(kEntry + 16u, 0u);          // delay-slot nop
  execution.bindAuthenticatedImage(GuestImage::Menu, image, kRange);
  crashbash::runtime::registerNativeOverride(core, GuestImage::Menu, kEntry, "menu-loop", nativeOriginal);
  core.r[31] = kReturn;
  core.r[8] = 1000u;
  const auto bounded = execution.original(GuestImage::Menu, kEntry, psx::cpu::ExecutionBudget::fromCycles(100u));
  CHECK_EQ(bounded.reason, psx::cpu::ExecutionExitReason::BudgetExhausted);
  CHECK(core.r[8] < 1000u);
  CHECK(core.r[8] > 0u);
  core.r[8] = 3u;
  const auto completed = execution.original(GuestImage::Menu, kEntry, psx::cpu::ExecutionBudget::fromCycles(100u));
  CHECK_EQ(completed.reason, psx::cpu::ExecutionExitReason::GuestReturn);
  CHECK_EQ(core.r[8], 0u);
}

// A counted loop, then r[2] = 42 and a return, with no nested `jal` (it would overwrite r[31]).
// Covers the wrapper's contract: return value, return address, caller registers kept, and the
// original's override suppression released. Resumed-original suppression is not observable here.
void test_bounded_call_ends_at_its_measured_return_address() {
  Runtime runtime;
  auto game = makeGame(runtime);
  Core &core = game->core;
  GuestExecution &execution = context(core);
  const auto image = core.imageCatalog().activate("menu", kRange, 1u);
  constexpr std::uint32_t kBody = 0x80010030u;
  core.mem_w32(kBody, 0x24080005u);       // addiu t0, zero, 5
  core.mem_w32(kBody + 4u, 0x2508ffffu);  // addiu t0, t0, -1
  core.mem_w32(kBody + 8u, 0x1500fffeu);  // bne t0, zero, kBody + 4u
  core.mem_w32(kBody + 12u, 0u);          // delay-slot nop
  core.mem_w32(kBody + 16u, 0x2402002au); // addiu v0, zero, 42
  core.mem_w32(kBody + 20u, 0x03e00008u); // jr ra
  core.mem_w32(kBody + 24u, 0u);          // delay-slot nop
  execution.bindAuthenticatedImage(GuestImage::Menu, image, kRange);
  core.r[31] = kReturn;
  core.r[29] = 0x801ff000u;
  // The product services pending work before every guest call; this harness has no frame driver.
  core.pending_work = 0;
  core.r[2] = 0u;
  core.r[8] = 0u;

  const std::uint32_t plain =
      crashbash::runtime::runGuestCallToReturn(core, kBody, kReturn, "test plain guest call", std::nullopt);
  CHECK_EQ(plain, 42u);
  CHECK_EQ(core.r[2], 42u);
  CHECK_EQ(core.r[8], 0u);
  CHECK_EQ(core.r[31], kReturn);
  CHECK_EQ(core.r[29], 0x801ff000u);

  // The same body run as the original of an installed key.
  crashbash::runtime::registerNativeOverride(core, GuestImage::Menu, kEntry, "menu-value", nativeValue);
  const psx::cpu::NativeKey key{image, kEntry};
  core.pending_work = 0;
  core.r[2] = 0u;
  core.r[8] = 0u;
  const std::uint32_t original =
      crashbash::runtime::runGuestCallToReturn(core, kBody, kReturn, "test original call", key);
  CHECK_EQ(original, 42u);
  CHECK_EQ(core.r[2], 42u);
  CHECK_EQ(core.r[8], 0u);
  CHECK_EQ(core.r[31], kReturn);
  // The suppression scope is released on return, so the key is intercepted again.
  CHECK(core.nativeDispatcher().intercepts({image, kEntry}));
}

void test_invalid_replacement_preserves_binding_and_cores_are_isolated() {
  Runtime runtime;
  auto first = makeGame(runtime);
  auto second = makeGame(runtime);
  Core &core = first->core;
  auto &execution = context(core);
  const auto image = core.imageCatalog().activate("menu", kRange, 1u);
  execution.registerOverride(GuestImage::Menu, kEntry, "value", nativeValue);
  execution.bindAuthenticatedImage(GuestImage::Menu, image, kRange);
  bool refused = false;
  try {
    execution.bindAuthenticatedImage(GuestImage::Menu, {image.id, image.generation + 1u}, kRange);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  CHECK(refused);
  CHECK(core.nativeDispatcher().isInstalled({image, kEntry}));
  CHECK(execution.activeKey(GuestImage::Menu, kEntry).has_value());
  CHECK(!context(second->core).activeKey(GuestImage::Menu, kEntry));
  CHECK(!second->core.nativeDispatcher().isInstalled({image, kEntry}));
  refused = false;
  try {
    execution.registerOverride(GuestImage::Menu, kEntry, "duplicate", nativeValue);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  CHECK(refused);
  refused = false;
  try {
    execution.registerOverride(GuestImage::Menu, kRange.end, "outside", nativeValue);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  CHECK(refused);
  core.imageCatalog().deactivate(image);
  CHECK(!execution.activeKey(GuestImage::Menu, kEntry));
}

// Restoring a state replaces the target session's image identities: resident, BOOT with the nested
// slot retired, and DAT22510 in that slot, restored over a session with MENU there.
void test_restored_identities_replace_the_session_and_keep_their_retired_ranges() {
  constexpr GuestAddressRange kResident{0x1000u, 0x2000u};
  constexpr GuestAddressRange kBoot{0x10000u, 0x10400u};
  constexpr GuestAddressRange kSlot{0x10200u, 0x10400u};
  constexpr std::uint32_t kPolarEntry = 0x80010200u;
  Runtime runtime;
  auto game = makeGame(runtime);
  Core &core = game->core;
  GuestExecution &execution = context(core);
  execution.registerOverride(GuestImage::Dat22510, kPolarEntry, "polar-value", nativeValue);

  execution.bindAuthenticatedImage(
      GuestImage::Resident, core.imageCatalog().activate("resident", kResident, 7u), kResident);
  execution.bindAuthenticatedImage(GuestImage::Boot, core.imageCatalog().activate("boot", kBoot, 8u), kBoot);
  execution.retireImagesOverlapping(kSlot);
  const auto savedPolar = core.imageCatalog().activate("polar", kSlot, 9u);
  execution.bindAuthenticatedImage(GuestImage::Dat22510, savedPolar, kSlot);

  psx::state::BlobWriter out;
  crashbash::runtime::writeBoundImages(out, execution.boundImages());
  psx::state::BlobReader in(out.bytesOut());
  std::string error;
  const auto records = crashbash::runtime::readBoundImages(in, error);
  CHECK_MSG(records.has_value(), error.c_str());
  CHECK_EQ(records->size(), 3u);

  // The session moves on: MENU replaces DAT22510 in the slot.
  execution.retireImagesOverlapping(kSlot);
  const auto menu = core.imageCatalog().activate("menu", kSlot, 10u);
  execution.bindAuthenticatedImage(GuestImage::Menu, menu, kSlot);
  CHECK(!core.nativeDispatcher().isInstalled({savedPolar, kPolarEntry}));

  execution.restoreBoundImages(*records);
  CHECK_EQ(core.imageCatalog().activeCount(), 3u);
  CHECK(execution.ownsEveryActiveResidency());
  const auto polar = core.currentImageIdentity(kPolarEntry);
  CHECK(polar.has_value());
  CHECK(*polar != menu);
  CHECK(*polar != savedPolar); // a fresh generation: nothing keyed to the old one can match it
  CHECK(core.imageCatalog().describe(*polar)->name == "polar");
  CHECK(core.nativeDispatcher().isInstalled({*polar, kPolarEntry}));
  CHECK(execution.activeKey(GuestImage::Dat22510, kPolarEntry).has_value());
  CHECK(!execution.activeKey(GuestImage::Menu, kPolarEntry));
  // BOOT comes back with the slot still retired, so the slot has exactly one owner.
  const auto boot = core.currentImageIdentity(0x80010000u);
  CHECK(boot.has_value());
  CHECK(core.imageCatalog().describe(*boot)->name == "boot");
  CHECK_EQ(core.imageCatalog().describe(*boot)->ranges.size(), 1u);
  CHECK(core.imageCatalog().describe(*boot)->ranges[0].end == kSlot.begin);
  CHECK(core.currentImageIdentity(0x80001000u).has_value());
}

// The reader refuses anything a live title could not have written, before restoring anything.
void test_image_identity_records_refuse_what_no_title_writes() {
  using crashbash::runtime::BoundImageRecord;
  const BoundImageRecord resident{GuestImage::Resident, "resident", 7u, {0x1000u, 0x2000u}, {{0x1000u, 0x2000u}}};
  const auto refused = [](const std::vector<BoundImageRecord> &records) {
    psx::state::BlobWriter out;
    crashbash::runtime::writeBoundImages(out, records);
    psx::state::BlobReader in(out.bytesOut());
    std::string error;
    const bool rejected = !crashbash::runtime::readBoundImages(in, error).has_value();
    return rejected && !error.empty();
  };
  CHECK(!refused({resident}));
  CHECK(refused({}));
  CHECK(refused({{GuestImage::Boot, "boot", 8u, {0x10000u, 0x10400u}, {{0x10000u, 0x10200u}}}}));
  CHECK(refused({resident, {GuestImage::Boot, "boot", 8u, {0x10000u, 0x10400u}, {}}}));
  CHECK(refused(
      {resident, {GuestImage::Boot, "boot", 8u, {0x10000u, 0x10400u}, {{0x10100u, 0x10300u}, {0x10200u, 0x10400u}}}}));
  CHECK(refused({resident, {GuestImage::Boot, "boot", 8u, {0x10000u, 0x10400u}, {{0x10000u, 0x10500u}}}}));
  CHECK(refused({resident, {GuestImage::Boot, "boot", 8u, {0x1F0000u, 0x210000u}, {{0x1F0000u, 0x200000u}}}}));
  CHECK(refused({resident, resident}));
  BoundImageRecord unknown = resident;
  unknown.image = static_cast<GuestImage>(static_cast<std::uint32_t>(GuestImage::Dat28382) + 1u);
  CHECK(refused({resident, unknown}));

  psx::state::BlobWriter out;
  crashbash::runtime::writeBoundImages(out, {resident});
  std::vector<std::uint8_t> truncated = out.bytesOut();
  truncated.pop_back();
  psx::state::BlobReader in(truncated);
  std::string error;
  CHECK(!crashbash::runtime::readBoundImages(in, error).has_value());
}

} // namespace

int main() {
  RUN(pending_registration_reload_and_wrong_image_refusal);
  RUN(original_uses_dynarec_and_restores_interception);
  RUN(original_budget_exit_exposes_live_loop_state);
  RUN(bounded_call_ends_at_its_measured_return_address);
  RUN(invalid_replacement_preserves_binding_and_cores_are_isolated);
  RUN(restored_identities_replace_the_session_and_keep_their_retired_ranges);
  RUN(image_identity_records_refuse_what_no_title_writes);
  return pt_summary();
}
